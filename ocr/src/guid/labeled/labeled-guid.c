/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_GUID_LABELED

#include "debug.h"
#include "guid/labeled/labeled-guid.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"

#define DEBUG_TYPE GUID

// Default hashtable's number of buckets
//PERF: This parameter heavily impacts the GUID provider scalability !
#define DEFAULT_NB_BUCKETS 1000

// Guid is composed of : (1/0 LOCID KIND COUNTER)
// The 1 at the top is if this is a "reserved" GUID (for checking purposes)
#define GUID_BIT_SIZE 64
#define GUID_LOCID_SIZE 7 // Warning! 2^7 locId max, bump that up for more.
#define GUID_KIND_SIZE 5 // Warning! check ocrGuidKind struct definition for correct size

#define GUID_COUNTER_SIZE (GUID_BIT_SIZE-(GUID_KIND_SIZE+GUID_LOCID_SIZE+1))
#define GUID_COUNTER_MASK ((((u64)1)<<GUID_COUNTER_SIZE)-1)

#define GUID_LOCID_MASK (((((u64)1)<<GUID_LOCID_SIZE)-1)<<(GUID_COUNTER_SIZE+GUID_KIND_SIZE))
#define GUID_LOCID_SHIFT_RIGHT (GUID_KIND_SIZE + GUID_COUNTER_SIZE)

#define GUID_KIND_MASK (((((u64)1)<<GUID_KIND_SIZE)-1)<< GUID_COUNTER_SIZE)
#define GUID_KIND_SHIFT_RIGHT (GUID_COUNTER_SIZE)

#define IS_RESERVED_GUID(guid) ((guid & 0x8000000000000000ULL) != 0ULL)
#define KIND_LOCATION 0
#define LOCID_LOCATION (GUID_KIND_SIZE)

// GUID 'id' counter, atomically incr when a new GUID is requested
static u64 guidCounter = 0;
// Counter for the reserved part of the GUIDs
static u64 guidReservedCounter = 0;

void labeledGuidDestruct(ocrGuidProvider_t* self) {
    //destructHashtable(((ocrGuidProviderLabeled_t *) self)->guidImplTable);
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 labeledGuidSwitchRunlevel(ocrGuidProvider_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                             phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        // Nothing
        break;
    case RL_PD_OK:
        if ((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        // Nothing to do
        if ((properties & RL_TEAR_DOWN) && RL_IS_FIRST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
            // What could the map contain at that point ?
            // - Non-freed OCR objects from the user program.
            // - GUIDs internally used by the runtime (module's guids)
            // Since this is below GUID_OK, nobody should have access to those GUIDs
            // anymore and we could dispose of them safely.
            // Note: - Do we want (and can we) destroy user objects ? i.e. need to
            //       call their specific destructors which may not work in MEM_OK ?
            //       - If there are any runtime GUID not deallocated then they should
            //       be considered as leaking memory.
            destructHashtable(((ocrGuidProviderLabeled_t *) self)->guidImplTable, NULL);
        }
        break;
    case RL_GUID_OK:
        ASSERT(self->pd == PD);
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(PD, RL_GUID_OK, phase)) {
            //Initialize the map now that we have an assigned policy domain
            ocrGuidProviderLabeled_t * derived = (ocrGuidProviderLabeled_t *) self;
            derived->guidImplTable = newHashtableBucketLockedModulo(PD, DEFAULT_NB_BUCKETS);
        }
        break;
    case RL_COMPUTE_OK:
        // We can allocate our map here because the memory is up
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

/**
 * @brief Utility function to extract a kind from a GUID.
 */
static ocrGuidKind getKindFromGuid(ocrGuid_t guid) {
    return (ocrGuidKind) ((guid & GUID_KIND_MASK) >> GUID_KIND_SHIFT_RIGHT);
}

/**
 * @brief Utility function to extract a kind from a GUID.
 */
static u64 extractLocIdFromGuid(ocrGuid_t guid) {
    return (u64) ((guid & GUID_LOCID_MASK) >> GUID_LOCID_SHIFT_RIGHT);
}

static ocrLocation_t locIdtoLocation(u64 locId) {
    //BUG #605 Locations spec: We assume there will be a mapping
    //between a location and an 'id' stored in the guid. For now identity.
    return (ocrLocation_t) (locId);
}

static u64 locationToLocId(ocrLocation_t location) {
    //BUG #605 Locations spec: We assume there will be a mapping
    //between a location and an 'id' stored in the guid. For now identity.
    u64 locId = (u64) ((int)location);
    // Make sure we're not overflowing location size
    ASSERT((locId < (1<<GUID_LOCID_SIZE)) && "GUID location ID overflows");
    return locId;
}

/**
 * @brief Utility function to generate a new GUID.
 */
static u64 generateNextGuid(ocrGuidProvider_t* self, ocrGuidKind kind) {
    u64 locId = (u64) locationToLocId(self->pd->myLocation);
    u64 locIdShifted = locId << LOCID_LOCATION;
    u64 kindShifted = kind << KIND_LOCATION;
    u64 guid = (locIdShifted | kindShifted) << GUID_COUNTER_SIZE;
    //PERF: Can privatize the counter by working out something with thread-id
    u64 newCount = hal_xadd64(&guidCounter, 1);
    // double check if we overflow the guid's counter size
    ASSERT((newCount + 1 < ((u64)1<<GUID_COUNTER_SIZE)) && "GUID counter overflows");
    guid |= newCount;
    DPRINTF(DEBUG_LVL_VVERB, "LabeledGUID generated GUID 0x%lx\n", guid);
    return guid;
}

u8 labeledGuidReserve(ocrGuidProvider_t *self, ocrGuid_t *startGuid, u64* skipGuid,
                      u64 numberGuids, ocrGuidKind guidType) {
    // We just return a range using our "header" (location, etc) just like for
    // generateNextGuid
    // ocrGuidType_t and ocrGuidKind should be the same (there are more GuidKind but
    // the ones that are the same should match)
    u64 locId = (u64) locationToLocId(self->pd->myLocation);
    u64 locIdShifted = locId << LOCID_LOCATION;
    u64 kindShifted = guidType << KIND_LOCATION;
    *startGuid = ((1 << (GUID_LOCID_SIZE + LOCID_LOCATION)) | locIdShifted | kindShifted) <<
        GUID_COUNTER_SIZE;

    *skipGuid = 1; // Each GUID will just increment by 1
    u64 firstCount = hal_xadd64(&guidReservedCounter, numberGuids);
    ASSERT(firstCount  + numberGuids < (u64)1<<GUID_COUNTER_SIZE);
    *startGuid |= firstCount;
    DPRINTF(DEBUG_LVL_VVERB, "LabeledGUID reserved a range for %lu GUIDs starting at 0x%lx\n",
            numberGuids, *startGuid);
    return 0;
}

u8 labeledGuidUnreserve(ocrGuidProvider_t *self, ocrGuid_t startGuid, u64 skipGuid,
                        u64 numberGuids) {
    // We do not do anything (we don't reclaim right now)
    return 0;
}

/**
 * @brief Generate a guid for 'val' by increasing the guid counter.
 */
u8 labeledGuidGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
    // Here no need to allocate
    u64 newGuid = generateNextGuid(self, kind);
    DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: insert into hash table 0x%lx -> 0x%lx\n", newGuid, val);
    hashtableConcBucketLockedPut(((ocrGuidProviderLabeled_t *) self)->guidImplTable, (void *) newGuid, (void *) val);
    *guid = (ocrGuid_t) newGuid;
    return 0;
}

u8 labeledGuidCreateGuid(ocrGuidProvider_t* self, ocrFatGuid_t *fguid, u64 size, ocrGuidKind kind, u32 properties) {

    if(properties & GUID_PROP_IS_LABELED) {
        // We need to use the GUID provided; make sure it is non null and reserved
        ASSERT((fguid->guid != NULL_GUID) && IS_RESERVED_GUID(fguid->guid));

        // We need to fix this: ie: return a code saying we can't do the reservation
        // Ideally, we would either forward to the responsible party or return something
        // so the PD knows what to do. This is going to take a lot more infrastructure
        // change so we'll punt for now
        // Related to BUG #535 and to BUG #536
        ASSERT(extractLocIdFromGuid(fguid->guid) == locationToLocId(self->pd->myLocation));

        // Other sanity check
        ASSERT(getKindFromGuid(fguid->guid) == kind); // Kind properly encoded
        ASSERT((fguid->guid & GUID_COUNTER_MASK) < guidReservedCounter); // Range actually reserved
    }
    ocrPolicyDomain_t *policy = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&policy, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(size) = size; // allocate 'size' payload as metadata
    PD_MSG_FIELD_I(properties) = 0;
    PD_MSG_FIELD_I(type) = GUID_MEMTYPE;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));
    void * ptr = (void *)PD_MSG_FIELD_O(ptr);

    // Update the fat GUID's metaDataPtr
    fguid->metaDataPtr = ptr;
    ASSERT(ptr);
#undef PD_TYPE
    (*(ocrGuid_t*)ptr) = NULL_GUID; // The first field is always the GUID, either directly as ocrGuid_t or a ocrFatGuid_t
                                    // This is used to determine if a GUID metadata is "ready". See bug #627
    hal_fence(); // Make sure the ptr update is visible before we update the hash table
    if(properties & GUID_PROP_IS_LABELED) {
        if((properties & GUID_PROP_CHECK) == GUID_PROP_CHECK) {
            // We need to actually check things
            DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: try insert into hash table 0x%lx -> 0x%lx\n", fguid->guid, ptr);
            void *value = hashtableConcBucketLockedTryPut(
                ((ocrGuidProviderLabeled_t*)self)->guidImplTable,
                (void*)(fguid->guid), ptr);
            if(value != ptr) {
                DPRINTF(DEBUG_LVL_VVERB, "LabeledGUID: FAILED to insert (got 0x%lx instead of 0x%lx)\n",
                        value, ptr);
                // Fail; already exists
                fguid->metaDataPtr = value; // Do we need to return this?
                // We now need to free the memory we allocated
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_MEM_UNALLOC
                msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(allocatingPD.guid) = NULL_GUID;
                PD_MSG_FIELD_I(allocator.guid) = NULL_GUID;
                PD_MSG_FIELD_I(ptr) = ptr;
                PD_MSG_FIELD_I(type) = GUID_MEMTYPE;
                PD_MSG_FIELD_I(properties) = 0;
                RESULT_PROPAGATE(policy->fcts.processMessage(policy, &msg, true));
#undef PD_TYPE
                // Bug #627: We do not return OCR_EGUIDEXISTS until the GUID is valid. We test this
                // by looking at the first field of ptr and waiting for it to be the GUID value (meaning the
                // object has been initialized
                while(*(volatile ocrGuid_t*)value != fguid->guid) ;
                hal_fence(); // May be overkill but there is a race that I don't get
                return OCR_EGUIDEXISTS;
            }
        } else if((properties & GUID_PROP_BLOCK) == GUID_PROP_BLOCK) {
            void* value = NULL;
            DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: force insert into hash table 0x%lx -> 0x%lx\n", fguid->guid, ptr);
            do {
                value = hashtableConcBucketLockedTryPut(
                    ((ocrGuidProviderLabeled_t*)self)->guidImplTable,
                    (void*)(fguid->guid), ptr);
            } while(value != ptr);
        } else {
            // "Trust me" mode. We insert into the hashtable
            DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: trust insert into hash table 0x%lx -> 0x%lx\n", fguid->guid, ptr);
            hashtableConcBucketLockedPut(((ocrGuidProviderLabeled_t*)self)->guidImplTable,
                                         (void*)(fguid->guid), ptr);
        }
    } else {
        labeledGuidGetGuid(self, &(fguid->guid), (u64)(fguid->metaDataPtr), kind);
    }
#undef PD_MSG
    DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: create GUID: 0x%lx -> 0x%lx\n", fguid->guid, fguid->metaDataPtr);
    return 0;
}

/**
 * @brief Returns the value associated with a guid and its kind if requested.
 */
u8 labeledGuidGetVal(ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind) {
    *val = (u64) hashtableConcBucketLockedGet(((ocrGuidProviderLabeled_t *) self)->guidImplTable, (void *) guid);
    DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: got val for GUID 0x%lx: 0x%lx\n", guid, *val);
    if(*val == (u64)NULL) {
        // Does not exist in the hashtable
        if(kind) {
            *kind = getKindFromGuid(guid);
        }
    } else {
        // Bug #627: We do not return until the GUID is valid. We test this
        // by looking at the first field of ptr and waiting for it to be the GUID value (meaning the
        // object has been initialized
        if(IS_RESERVED_GUID(guid)) {
            while(*((volatile ocrGuid_t*)(*val)) != guid) ;
            hal_fence(); // May be overkill but there is a race that I don't get
        }
        if(kind) {
            *kind = getKindFromGuid(guid);
        }
    }

    return 0;
}

/**
 * @brief Get the 'kind' of the guid pointed object.
 */
u8 labeledGuidGetKind(ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind) {
    *kind = getKindFromGuid(guid);
    return 0;
}

/**
 * @brief Resolve location of a GUID
 */
u8 labeledGuidGetLocation(ocrGuidProvider_t* self, ocrGuid_t guid, ocrLocation_t* location) {
    //Resolve the actual location of the GUID
    *location = (ocrLocation_t) locIdtoLocation(extractLocIdFromGuid(guid));
    return 0;
}

/**
 * @brief Associate an already existing GUID to a value.
 * This is useful in the context of distributed-OCR to register
 * a local metadata represent for a foreign GUID.
 */
u8 labeledGuidRegisterGuid(ocrGuidProvider_t* self, ocrGuid_t guid, u64 val) {
    DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: register GUID 0x%lx -> 0x%lx\n", guid, val);
    hashtableConcBucketLockedPut(((ocrGuidProviderLabeled_t *) self)->guidImplTable, (void *) guid, (void *) val);
    return 0;
}

u8 labeledGuidReleaseGuid(ocrGuidProvider_t *self, ocrFatGuid_t fatGuid, bool releaseVal) {
    DPRINTF(DEBUG_LVL_VERB, "LabeledGUID: release GUID 0x%lx\n", fatGuid.guid);
    ocrGuid_t guid = fatGuid.guid;
    // We *first* remove the GUID from the hashtable otherwise the following race
    // could occur:
    //   - free the metadata
    //   - another thread trying to create the same GUID creates the metadata at the *same* address
    //   - the other thread tries to insert, this succeeds immediately since it's
    //     the same value for the pointer (already in the hashtable)
    //   - this function removes the value from the hashtable
    //   => the creator thinks all is swell but the data was actually *removed*
    ocrGuidProviderLabeled_t * derived = (ocrGuidProviderLabeled_t *) self;
    RESULT_ASSERT(hashtableConcBucketLockedRemove(derived->guidImplTable, (void *)guid, NULL), ==, true);
    // If there's metaData associated with guid we need to deallocate memory
    if(releaseVal && (fatGuid.metaDataPtr != NULL)) {
        PD_MSG_STACK(msg);
        ocrPolicyDomain_t *policy = NULL;
        getCurrentEnv(&policy, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_UNALLOC
        msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_I(allocatingPD.guid) = NULL_GUID;
        PD_MSG_FIELD_I(allocatingPD.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(allocator.guid) = NULL_GUID;
        PD_MSG_FIELD_I(allocator.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(ptr) = fatGuid.metaDataPtr;
        PD_MSG_FIELD_I(type) = GUID_MEMTYPE;
        PD_MSG_FIELD_I(properties) = 0;
        RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));
#undef PD_MSG
#undef PD_TYPE
    }
    return 0;
}

static ocrGuidProvider_t* newGuidProviderLabeled(ocrGuidProviderFactory_t *factory,
                                                 ocrParamList_t *perInstance) {
    ocrGuidProvider_t *base = (ocrGuidProvider_t*) runtimeChunkAlloc(sizeof(ocrGuidProviderLabeled_t), PERSISTENT_CHUNK);
    base->fcts = factory->providerFcts;
    base->pd = NULL;
    base->id = factory->factoryId;
    return base;
}

/****************************************************/
/* OCR GUID PROVIDER LABELED FACTORY                */
/****************************************************/

static void destructGuidProviderFactoryLabeled(ocrGuidProviderFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrGuidProviderFactory_t *newGuidProviderFactoryLabeled(ocrParamList_t *typeArg, u32 factoryId) {
    ocrGuidProviderFactory_t *base = (ocrGuidProviderFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrGuidProviderFactoryLabeled_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newGuidProviderLabeled;
    base->destruct = &destructGuidProviderFactoryLabeled;
    base->factoryId = factoryId;
    base->providerFcts.destruct = FUNC_ADDR(void (*)(ocrGuidProvider_t*), labeledGuidDestruct);
    base->providerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64),
        labeledGuidSwitchRunlevel);
    base->providerFcts.guidReserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64*, u64, ocrGuidKind), labeledGuidReserve);
    base->providerFcts.guidUnreserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64, u64), labeledGuidUnreserve);
    base->providerFcts.getGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64, ocrGuidKind), labeledGuidGetGuid);
    base->providerFcts.createGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t*, u64, ocrGuidKind, u32), labeledGuidCreateGuid);
    base->providerFcts.getVal = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64*, ocrGuidKind*), labeledGuidGetVal);
    base->providerFcts.getKind = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrGuidKind*), labeledGuidGetKind);
    base->providerFcts.getLocation = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrLocation_t*), labeledGuidGetLocation);
    base->providerFcts.registerGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64), labeledGuidRegisterGuid);
    base->providerFcts.releaseGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t, bool), labeledGuidReleaseGuid);

    return base;
}

#endif /* ENABLE_GUID_LABELED */
