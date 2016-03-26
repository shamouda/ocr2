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
#include "ocr-errors.h"

#define DEBUG_TYPE GUID

// Default hashtable's number of buckets
//PERF: This parameter heavily impacts the GUID provider scalability !
#ifndef GUID_PROVIDER_NB_BUCKETS
#define GUID_PROVIDER_NB_BUCKETS 10000
#endif

// Guid is composed of : (LOCID KIND COUNTER)
#define GUID_BIT_SIZE 64
#ifndef GUID_PROVIDER_LOCID_SIZE
#define GUID_LOCID_SIZE 7 // Warning! 2^7 locId max, bump that up for more.
#else
#define GUID_LOCID_SIZE GUID_PROVIDER_LOCID_SIZE
#endif
#define GUID_KIND_SIZE 5 // Warning! check ocrGuidKind struct definition for correct size

#ifdef GUID_PROVIDER_WID_INGUID
#ifndef GUID_WID_SIZE
#define GUID_WID_SIZE 4
#else
#define GUID_WID_SIZE GUID_PROVIDER_WID_SIZE
#endif

// Here Guid is composed of : (LOCID KIND WID COUNTER)
#define GUID_COUNTER_SIZE (GUID_BIT_SIZE-(GUID_LOCID_SIZE+GUID_KIND_SIZE+GUID_WID_SIZE))
#define GUID_WID_MASK (((((u64)1)<<GUID_WID_SIZE)-1)<<(GUID_COUNTER_SIZE))
#define GUID_COUNTER_MASK ((((u64)1)<<(GUID_COUNTER_SIZE))-1)
#define GUID_LOCID_MASK (((((u64)1)<<GUID_LOCID_SIZE)-1)<<(GUID_COUNTER_SIZE+GUID_WID_SIZE+GUID_KIND_SIZE))
#define GUID_LOCID_SHIFT_RIGHT (GUID_BIT_SIZE-GUID_LOCID_SIZE)
#define GUID_KIND_MASK (((((u64)1)<<GUID_KIND_SIZE)-1)<<(GUID_COUNTER_SIZE+GUID_WID_SIZE))
#define GUID_KIND_SHIFT_RIGHT (GUID_LOCID_SHIFT_RIGHT-GUID_KIND_SIZE)
#define GUID_WID_SHIFT_RIGHT (GUID_KIND_SHIFT_RIGHT-GUID_WID_SIZE)
#define WID_LOCATION (0)
#define KIND_LOCATION (GUID_WID_SIZE)
#define LOCID_LOCATION (KIND_LOCATION+GUID_KIND_SIZE)
#else
#define GUID_COUNTER_SIZE (GUID_BIT_SIZE-(GUID_KIND_SIZE+GUID_LOCID_SIZE))
#define GUID_COUNTER_MASK ((((u64)1)<<(GUID_COUNTER_SIZE))-1)
#define GUID_LOCID_MASK (((((u64)1)<<GUID_LOCID_SIZE)-1)<<(GUID_COUNTER_SIZE+GUID_KIND_SIZE))
#define GUID_LOCID_SHIFT_RIGHT (GUID_BIT_SIZE-GUID_LOCID_SIZE)
#define GUID_KIND_MASK (((((u64)1)<<GUID_KIND_SIZE)-1)<<GUID_COUNTER_SIZE)
#define GUID_KIND_SHIFT_RIGHT (GUID_LOCID_SHIFT_RIGHT-GUID_KIND_SIZE)
#define KIND_LOCATION (0)
#define LOCID_LOCATION GUID_KIND_SIZE
#endif

#ifdef GUID_PROVIDER_CUSTOM_MAP
// Set -DGUID_PROVIDER_CUSTOM_MAP and put other #ifdef for alternate implementation here
#else
#define GP_RESOLVE_HASHTABLE(hashtable, key) hashtable
#define GP_HASHTABLE_CREATE_MODULO newHashtableBucketLocked
#define GP_HASHTABLE_DESTRUCT(hashtable, key, entryDealloc, deallocParam) destructHashtableBucketLocked(hashtable, entryDealloc, deallocParam)
#define GP_HASHTABLE_GET(hashtable, key) hashtableConcBucketLockedGet(GP_RESOLVE_HASHTABLE(hashtable,key), key)
#define GP_HASHTABLE_PUT(hashtable, key, value) hashtableConcBucketLockedPut(GP_RESOLVE_HASHTABLE(hashtable,key), key, value)
#define GP_HASHTABLE_DEL(hashtable, key, valueBack) hashtableConcBucketLockedRemove(GP_RESOLVE_HASHTABLE(hashtable,key), key, valueBack)
#endif

#ifdef GUID_PROVIDER_WID_INGUID
#define CACHE_SIZE 8
static u64 guidCounters[(((u64)1)<<GUID_WID_SIZE)*CACHE_SIZE];
#else
// GUID 'id' counter, atomically incr when a new GUID is requested
static u64 guidCounter = 0;
#endif

#ifdef GUID_PROVIDER_DESTRUCT_CHECK
// Fwd declaration
static ocrGuidKind getKindFromGuid(ocrGuid_t guid);

void countedMapHashmapEntryDestructChecker(void * key, void * value, void * deallocParam) {
    ocrGuid_t guid;
#if GUID_BIT_COUNT == 64
    guid.guid = (u64) key;
#elif GUID_BIT_COUNT == 128
    guid.upper = 0x0;
    guid.lower = (u64) key;
#endif
    ((u32*)deallocParam)[getKindFromGuid(guid)]++;
#ifdef GUID_PROVIDER_DESTRUCT_CHECK_VERBOSE
    DPRINTF(DEBUG_LVL_WARN, "Remnant GUID "GUIDF" of kind %s still registered on GUID provider\n", GUIDA(guid),  ocrGuidKindToChar(getKindFromGuid(guid)));
#endif
}
#endif

static u32 hashGuidCounterModulo(void * ptr, u32 nbBuckets) {
    u64 guid = (u64) ptr;
    return ((guid & GUID_COUNTER_MASK) % nbBuckets);
}

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
#ifdef GUID_PROVIDER_WID_INGUID
        u32 i = 0, ub = PD->workerCount;
        u64 max = ((u64)1<<GUID_COUNTER_SIZE);
        u64 incr = (max/ub);
        while (i < ub) {
            // Initialize to 'i' to distribute the count over the buckets. Helps with scalability.
            // This is knowing we use a modulo hash but is not hurting generally speaking...
            guidCounters[i*CACHE_SIZE] = incr*i;
            i++;
        }
#endif
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
#ifdef GUID_PROVIDER_DESTRUCT_CHECK
            deallocFct entryDeallocator = countedMapHashmapEntryDestructChecker;
            u32 guidTypeCounters[OCR_GUID_MAX];
            u32 i;
            for(i=0; i < OCR_GUID_MAX; i++) {
                guidTypeCounters[i] = 0;
            }
            void * deallocParam = (void *) guidTypeCounters;
#else
            deallocFct entryDeallocator = NULL;
            void * deallocParam = NULL;
#endif
            GP_HASHTABLE_DESTRUCT(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, NULL, entryDeallocator, deallocParam);
#ifdef GUID_PROVIDER_DESTRUCT_CHECK
            PRINTF("=========================\n");
            PRINTF("Remnant GUIDs summary:\n");
            for(i=0; i < OCR_GUID_MAX; i++) {
                if (guidTypeCounters[i] != 0) {
                    PRINTF("%s => %"PRIu32" instances\n", ocrGuidKindToChar(i), guidTypeCounters[i]);
                }
            }
            PRINTF("=========================\n");
#endif
        }
        break;
    case RL_GUID_OK:
        ASSERT(self->pd == PD);
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(PD, RL_GUID_OK, phase)) {
            //Initialize the map now that we have an assigned policy domain
            ocrGuidProviderCountedMap_t * derived = (ocrGuidProviderCountedMap_t *) self;
            derived->guidImplTable = GP_HASHTABLE_CREATE_MODULO(PD, GUID_PROVIDER_NB_BUCKETS, hashGuidCounterModulo);
#ifdef GUID_PROVIDER_WID_INGUID
            ASSERT(((PD->workerCount-1) < ((u64)1 << GUID_WID_SIZE)) && "GUID worker count overflows");
#endif
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
    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    return (ocrGuidKind) ((guid.guid & GUID_KIND_MASK) >> GUID_KIND_SHIFT_RIGHT);
#elif GUID_BIT_COUNT == 128
    return (ocrGuidKind) ((guid.lower & GUID_KIND_MASK) >> GUID_KIND_SHIFT_RIGHT);
#endif
}

/**
 * @brief Utility function to extract a kind from a GUID.
 */
static u64 extractLocIdFromGuid(ocrGuid_t guid) {
// See BUG #928 on GUID issues

#if GUID_BIT_COUNT == 64
    return (u64) ((guid.guid & GUID_LOCID_MASK) >> GUID_LOCID_SHIFT_RIGHT);
#elif GUID_BIT_COUNT == 128
    return (u64) ((guid.lower & GUID_LOCID_MASK) >> GUID_LOCID_SHIFT_RIGHT);
#endif
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
#ifdef GUID_PROVIDER_WID_INGUID
    ocrWorker_t * worker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    // ASSERT((worker != NULL) && "GUID-Provider Cannot identify currently executing worker");
    // GUIDs are generated before the current worker is setup.
    u64 wid = (worker == NULL) ? 0 : worker->id;
    u64 widShifted = (wid << WID_LOCATION);
    u64 guid = (locIdShifted | kindShifted | widShifted) << GUID_COUNTER_SIZE;
    u64 newCount = guidCounters[wid*CACHE_SIZE]++;
#else
    u64 guid = (locIdShifted | kindShifted) << GUID_COUNTER_SIZE;
    u64 newCount = hal_xadd64(&guidCounter, 1);
#endif
    // double check if we overflow the guid's counter size
    ASSERT((newCount + 1 < ((u64)1<<GUID_COUNTER_SIZE)) && "GUID counter overflows");
    guid |= newCount;
    return guid;
}

u8 countedMapGuidReserve(ocrGuidProvider_t *self, ocrGuid_t* startGuid, u64* skipGuid,
                         u64 numberGuids, ocrGuidKind guidType) {
    // Not supported; use labeled provider
    DPRINTF(DEBUG_LVL_WARN, "error: Must use labeled GUID provider for labeled GUID support, current is counted-map\n");
    ASSERT(false);
    return OCR_ENOTSUP;
}

u8 countedMapGuidUnreserve(ocrGuidProvider_t *self, ocrGuid_t startGuid, u64 skipGuid,
                           u64 numberGuids) {
    // Not supported; use labeled provider
    DPRINTF(DEBUG_LVL_WARN, "error: Must use labeled GUID provider for labeled GUID support, current is counted-map\n");
    ASSERT(false);
    return OCR_ENOTSUP;
}

/**
 * @brief Generate a guid for 'val' by increasing the guid counter.
 */
static u8 countedMapGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
    // Here no need to allocate
    u64 newGuid = generateNextGuid(self, kind);

    GP_HASHTABLE_PUT(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) newGuid, (void *) val);
    // See BUG #928 on GUID issues

#if GUID_BIT_COUNT == 64
    ocrGuid_t tempGuid = {.guid = newGuid};
    *guid = tempGuid;
#elif GUID_BIT_COUNT == 128
    ocrGuid_t tempGuid = {.lower = (u64)newGuid, .upper = 0x0};
    *guid = tempGuid;
#else
#error Unknown type of GUID
#endif
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
        DPRINTF(DEBUG_LVL_WARN, "error: Must use labeled GUID provider for labeled GUID support, current is counted-map\n");
        ASSERT(false);
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
    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    *val = (u64) GP_HASHTABLE_GET(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.guid);
#elif GUID_BIT_COUNT == 128
    *val = (u64) GP_HASHTABLE_GET(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.lower);
#else
#error Unknown type of GUID
#endif

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
    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    GP_HASHTABLE_PUT(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.guid, (void *) val);
#elif GUID_BIT_COUNT == 128
    GP_HASHTABLE_PUT(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.lower, (void *) val);
#else
#error Unknown type of GUID
#endif
    return 0;
}

/**
 * @brief Remove an already existing GUID and its associated value from the provider
 */
u8 countedMapUnregisterGuid(ocrGuidProvider_t* self, ocrGuid_t guid, u64 ** val) {
    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    GP_HASHTABLE_DEL(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.guid, (void **) val);
#elif GUID_BIT_COUNT == 128
    GP_HASHTABLE_DEL(((ocrGuidProviderCountedMap_t *) self)->guidImplTable, (void *) guid.lower, (void **) val);
#else
#error Unknown type of GUID
#endif
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
    // See BUG #928 on GUID issues
#if GUID_BIT_COUNT == 64
    GP_HASHTABLE_DEL(derived->guidImplTable, (void *)guid.guid, NULL);
#elif GUID_BIT_COUNT == 128
    GP_HASHTABLE_DEL(derived->guidImplTable, (void *) guid.lower, NULL);
#else
#error Unknown type of GUID
#endif
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
    base->providerFcts.unregisterGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64**), countedMapUnregisterGuid);
    base->providerFcts.releaseGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t, bool), countedMapReleaseGuid);
    return base;
}

#endif /* ENABLE_GUID_COUNTED_MAP */
