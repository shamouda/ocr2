/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_GUID_COUNTED_MAP

#include "debug.h"
#include "guid/counted/counted-map-guid.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"

#define DEBUG_TYPE GUID

// Default hashtable's number of buckets
//PERF: This parameter heavily impacts the GUID provider scalability !
#define DEFAULT_NB_BUCKETS 10000

// Guid is composed of : (LOCID KIND COUNTER)
#define GUID_BIT_SIZE 64
#define GUID_LOCID_SIZE 7 // Warning! 2^7 locId max, bump that up for more.
#define GUID_KIND_SIZE 5 // Warning! check ocrGuidKind struct definition for correct size

#define GUID_COUNTER_SIZE (GUID_BIT_SIZE-(GUID_KIND_SIZE+GUID_LOCID_SIZE))

#define GUID_LOCID_MASK (((((u64)1)<<GUID_LOCID_SIZE)-1)<<(GUID_COUNTER_SIZE+GUID_KIND_SIZE))
#define GUID_LOCID_SHIFT_RIGHT (GUID_BIT_SIZE-GUID_LOCID_SIZE)

#define GUID_KIND_MASK (((((u64)1)<<GUID_KIND_SIZE)-1)<< GUID_COUNTER_SIZE)
#define GUID_KIND_SHIFT_RIGHT (GUID_LOCID_SHIFT_RIGHT-GUID_KIND_SIZE)

#define KIND_LOCATION 0
#define LOCID_LOCATION GUID_KIND_SIZE

// GUID 'id' counter, atomically incr when a new GUID is requested
static u64 guidCounter = 0;

#ifdef COUNTED_MAP_DESTRUCT_CHECK
// Fwd declaration
static ocrGuidKind getKindFromGuid(ocrGuid_t guid);

void countedMapHashmapEntryDestructChecker(void * key, void * value) {
    ocrGuid_t guid = (ocrGuid_t) key;
    DPRINTF(DEBUG_LVL_WARN, "Remnant GUID 0x%x of kind %d still registered on GUID provider\n", guid, getKindFromGuid(guid));
}
#endif

u8 countedMapSwitchRunlevel(ocrGuidProvider_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
#ifdef COUNTED_MAP_DESTRUCT_CHECK
            deallocFct entryDeallocator = countedMapHashmapEntryDestructChecker;
#else
            deallocFct entryDeallocator = NULL;
#endif
            destructHashtableBucketLocked(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, entryDeallocator);
        }
        break;
    case RL_GUID_OK:
        ASSERT(self->pd == PD);
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(PD, RL_GUID_OK, phase)) {
            //Initialize the map now that we have an assigned policy domain
            ocrGuidProviderCountedMap_t * derived = (ocrGuidProviderCountedMap_t *) self;
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

void countedMapDestruct(ocrGuidProvider_t* self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

/**
 * @brief Utility function to extract a kind from a GUID.
 */
static ocrGuidKind getKindFromGuid(ocrGuid_t guid) {
    return (ocrGuidKind) ((guid & GUID_KIND_MASK) >> GUID_COUNTER_SIZE);
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
    ASSERT((newCount < ((u64)1<<GUID_COUNTER_SIZE)) && "GUID counter overflows");
    guid |= newCount;
    return guid;
}

u8 countedMapGuidReserve(ocrGuidProvider_t *self, ocrGuid_t* startGuid, u64* skipGuid,
                         u64 numberGuids, ocrGuidKind guidType) {
    // Non supported; use labeled provider
    ASSERT(0);
}

u8 countedMapGuidUnreserve(ocrGuidProvider_t *self, ocrGuid_t startGuid, u64 skipGuid,
                           u64 numberGuids) {
    // Non supported; use labeled provider
    ASSERT(0);
}

/**
 * @brief Generate a guid for 'val' by increasing the guid counter.
 */
static u8 countedMapGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
    // Here no need to allocate
    u64 newGuid = generateNextGuid(self, kind);
    hashtableConcBucketLockedPut(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) newGuid, (void *) val);
    *guid = (ocrGuid_t) newGuid;
    return 0;
}

/**
 * @brief Allocates a piece of memory that embeds both
 * the guid and some meta-data payload behind it
 * fatGuid's metaDataPtr will point to.
 */
u8 countedMapCreateGuid(ocrGuidProvider_t* self, ocrFatGuid_t *fguid, u64 size, ocrGuidKind kind, u32 properties) {
    if(properties & GUID_PROP_IS_LABELED) {
        // Not supported; use labeled provider
        ASSERT(0);
    }

    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *policy = NULL;
    getCurrentEnv(&policy, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(size) = size; // allocate 'size' payload as metadata
    PD_MSG_FIELD_I(properties) = 0;
    PD_MSG_FIELD_I(type) = GUID_MEMTYPE;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));

    void * ptr = (void *)PD_MSG_FIELD_O(ptr);
    // Fill in the GUID value and in the fatGuid
    // and registers its associated metadata ptr
    countedMapGetGuid(self, &(fguid->guid), (u64) ptr, kind);
    // Update the fat GUID's metaDataPtr
    fguid->metaDataPtr = ptr;
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

/**
 * @brief Returns the value associated with a guid and its kind if requested.
 */
static u8 countedMapGetVal(ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind) {
    *val = (u64) hashtableConcBucketLockedGet(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid);
    if(kind) {
        *kind = getKindFromGuid(guid);
    }
    return 0;
}

/**
 * @brief Get the 'kind' of the guid pointed object.
 */
static u8 countedMapGetKind(ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind) {
    *kind = getKindFromGuid(guid);
    return 0;
}

/**
 * @brief Resolve location of a GUID
 */
u8 countedMapGetLocation(ocrGuidProvider_t* self, ocrGuid_t guid, ocrLocation_t* location) {
    //Resolve the actual location of the GUID
    *location = (ocrLocation_t) locIdtoLocation(extractLocIdFromGuid(guid));
    return 0;
}

/**
 * @brief Associate an already existing GUID to a value.
 * This is useful in the context of distributed-OCR to register
 * a local metadata represent for a foreign GUID.
 */
u8 countedMapRegisterGuid(ocrGuidProvider_t* self, ocrGuid_t guid, u64 val) {
    hashtableConcBucketLockedPut(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid, (void *) val);
    return 0;
}

u8 countedMapReleaseGuid(ocrGuidProvider_t *self, ocrFatGuid_t fatGuid, bool releaseVal) {
    ocrGuid_t guid = fatGuid.guid;
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
    // In any case, we need to recycle the guid
    ocrGuidProviderCountedMap_t * derived = (ocrGuidProviderCountedMap_t *) self;
    hashtableConcBucketLockedRemove(derived->guidImplTable, (void *)guid, NULL);
    return 0;
}

static ocrGuidProvider_t* newGuidProviderCountedMap(ocrGuidProviderFactory_t *factory,
        ocrParamList_t *perInstance) {
    ocrGuidProvider_t *base = (ocrGuidProvider_t*) runtimeChunkAlloc(sizeof(ocrGuidProviderCountedMap_t), PERSISTENT_CHUNK);
    base->fcts = factory->providerFcts;
    base->pd = NULL;
    base->id = factory->factoryId;
    return base;
}

/****************************************************/
/* OCR GUID PROVIDER COUNTED MAP FACTORY            */
/****************************************************/

static void destructGuidProviderFactoryCountedMap(ocrGuidProviderFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrGuidProviderFactory_t *newGuidProviderFactoryCountedMap(ocrParamList_t *typeArg, u32 factoryId) {
    ocrGuidProviderFactory_t *base = (ocrGuidProviderFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrGuidProviderFactoryCountedMap_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newGuidProviderCountedMap;
    base->destruct = &destructGuidProviderFactoryCountedMap;
    base->factoryId = factoryId;

    base->providerFcts.destruct = FUNC_ADDR(void (*)(ocrGuidProvider_t*), countedMapDestruct);
    base->providerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), countedMapSwitchRunlevel);
    base->providerFcts.guidReserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64*, u64, ocrGuidKind), countedMapGuidReserve);
    base->providerFcts.guidUnreserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64, u64), countedMapGuidUnreserve);
    base->providerFcts.getGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64, ocrGuidKind), countedMapGetGuid);
    base->providerFcts.createGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t*, u64, ocrGuidKind, u32), countedMapCreateGuid);
    base->providerFcts.getVal = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64*, ocrGuidKind*), countedMapGetVal);
    base->providerFcts.getKind = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrGuidKind*), countedMapGetKind);
    base->providerFcts.getLocation = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrLocation_t*), countedMapGetLocation);
    base->providerFcts.registerGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64), countedMapRegisterGuid);
    base->providerFcts.releaseGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t, bool), countedMapReleaseGuid);
    return base;
}

#endif /* ENABLE_GUID_COUNTED_MAP */
