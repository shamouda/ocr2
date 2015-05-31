/**
 * @brief Trivial implementation of GUIDs
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_GUID_PTR

#include "debug.h"
#include "ocr-types.h"
#include "guid/ptr/ptr-guid.h"
#include "ocr-policy-domain.h"

#include "ocr-sysboot.h"

#define DEBUG_TYPE GUID

#ifdef HAL_FSIM_CE
#include "rmd-map.h"
#endif

typedef struct {
    ocrGuid_t guid;
    ocrGuidKind kind;
    ocrLocation_t location;
} ocrGuidImpl_t;

void ptrDestruct(ocrGuidProvider_t* self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}


u8 ptrSwitchRunlevel(ocrGuidProvider_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP)
            self->pd = PD;
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

u8 ptrGuidReserve(ocrGuidProvider_t *self, ocrGuid_t* startGuid, u64* skipGuid,
                  u64 numberGuids, ocrGuidKind guidType) {
    // Non supported; use labeled provider
    ASSERT(0);
    return 0;
}

u8 ptrGuidUnreserve(ocrGuidProvider_t *self, ocrGuid_t startGuid, u64 skipGuid,
                    u64 numberGuids) {
    // Non supported; use labeled provider
    ASSERT(0);
    return 0;
}

u8 ptrGetGuid(ocrGuidProvider_t* self, ocrGuid_t* guid, u64 val, ocrGuidKind kind) {
    ocrGuidImpl_t *guidInst = NULL;
    PD_MSG_STACK(msg);
    ocrTask_t *task = NULL;
    ocrPolicyDomain_t *policy = NULL; /* should be self->pd. There is an issue with TG-x86 though... */
    getCurrentEnv(&policy, NULL, &task, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(size) = sizeof(ocrGuidImpl_t);
    PD_MSG_FIELD_I(type) = GUID_MEMTYPE;
    PD_MSG_FIELD_I(properties) = 0;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));

    guidInst = (ocrGuidImpl_t *)PD_MSG_FIELD_O(ptr);

    guidInst->guid = (ocrGuid_t)val;
    guidInst->kind = kind;
    guidInst->location = self->pd->myLocation;
    *guid = (ocrGuid_t) guidInst;
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 ptrCreateGuid(ocrGuidProvider_t* self, ocrFatGuid_t *fguid, u64 size, ocrGuidKind kind, u32 properties) {
    if(properties & GUID_PROP_IS_LABELED) {
        ASSERT(0); // Not supported; use labeled provider
    }

    PD_MSG_STACK(msg);
    ocrTask_t *task = NULL;
    ocrPolicyDomain_t *policy = NULL;
    getCurrentEnv(&policy, NULL, &task, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_ALLOC
    msg.type = PD_MSG_MEM_ALLOC | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(size) = sizeof(ocrGuidImpl_t) + size;
    PD_MSG_FIELD_I(properties) = 0;
    PD_MSG_FIELD_I(type) = GUID_MEMTYPE;

    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));

    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *)PD_MSG_FIELD_O(ptr);
#ifdef HAL_FSIM_CE
    if((u64)PD_MSG_FIELD_O(ptr) < CE_MSR_BASE) // BUG #222: do this check properly
        guidInst = (ocrGuidImpl_t *) DR_CE_BASE(CHIP_FROM_ID(policy->myLocation),
                                                UNIT_FROM_ID(policy->myLocation),
                                                BLOCK_FROM_ID(policy->myLocation))
                                     + (u64)(PD_MSG_FIELD_O(ptr) - BR_CE_BASE);
#endif

    guidInst->guid = (ocrGuid_t)((u64)guidInst + sizeof(ocrGuidImpl_t));
    guidInst->kind = kind;
    guidInst->location = policy->myLocation;

    fguid->guid = (ocrGuid_t)guidInst;
    fguid->metaDataPtr = (void*)((u64)guidInst + sizeof(ocrGuidImpl_t));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 ptrGetVal(ocrGuidProvider_t* self, ocrGuid_t guid, u64* val, ocrGuidKind* kind) {
    ASSERT(guid != NULL_GUID);
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *val = (u64) guidInst->guid;
    if(kind)
        *kind = guidInst->kind;
    return 0;
}

u8 ptrGetKind(ocrGuidProvider_t* self, ocrGuid_t guid, ocrGuidKind* kind) {
    ASSERT(guid != NULL_GUID);
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *kind = guidInst->kind;
    return 0;
}

u8 ptrGetLocation(ocrGuidProvider_t* self, ocrGuid_t guid, ocrLocation_t* location) {
    ASSERT(guid != NULL_GUID);
    ocrGuidImpl_t * guidInst = (ocrGuidImpl_t *) guid;
    *location = guidInst->location;
    return 0;
}

u8 ptrRegisterGuid(ocrGuidProvider_t* self, ocrGuid_t guid, u64 val) {
    ASSERT(0); // Not supported
    return 0;
}

u8 ptrReleaseGuid(ocrGuidProvider_t *self, ocrFatGuid_t guid, bool releaseVal) {
    if(releaseVal) {
        ASSERT(guid.metaDataPtr);
        ASSERT((u64)guid.metaDataPtr == (u64)guid.guid + sizeof(ocrGuidImpl_t));
    }
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
    PD_MSG_FIELD_I(ptr) = ((void *) guid.guid);
    PD_MSG_FIELD_I(type) = GUID_MEMTYPE;
    PD_MSG_FIELD_I(properties) = 0;
    RESULT_PROPAGATE(policy->fcts.processMessage (policy, &msg, true));
#undef PD_MSG
#undef PD_TYPE

    return 0;
}

ocrGuidProvider_t* newGuidProviderPtr(ocrGuidProviderFactory_t *factory,
                                      ocrParamList_t *perInstance) {
    ocrGuidProvider_t *base = (ocrGuidProvider_t*)runtimeChunkAlloc(
                                  sizeof(ocrGuidProviderPtr_t), PERSISTENT_CHUNK);
    base->fcts = factory->providerFcts;
    base->pd = NULL;
    base->id = factory->factoryId;
    return base;
}

/****************************************************/
/* OCR GUID PROVIDER PTR FACTORY                    */
/****************************************************/

void destructGuidProviderFactoryPtr(ocrGuidProviderFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrGuidProviderFactory_t *newGuidProviderFactoryPtr(ocrParamList_t *typeArg, u32 factoryId) {
    ocrGuidProviderFactory_t *base = (ocrGuidProviderFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrGuidProviderFactoryPtr_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newGuidProviderPtr;
    base->destruct = &destructGuidProviderFactoryPtr;
    base->factoryId = factoryId;
    base->providerFcts.destruct = FUNC_ADDR(void (*)(ocrGuidProvider_t*), ptrDestruct);
    base->providerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), ptrSwitchRunlevel);
    base->providerFcts.guidReserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64*, u64, ocrGuidKind), ptrGuidReserve);
    base->providerFcts.guidUnreserve = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64, u64), ptrGuidUnreserve);
    base->providerFcts.getGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t*, u64, ocrGuidKind), ptrGetGuid);
    base->providerFcts.createGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t*, u64, ocrGuidKind, u32), ptrCreateGuid);
    base->providerFcts.getVal = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64*, ocrGuidKind*), ptrGetVal);
    base->providerFcts.getKind = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrGuidKind*), ptrGetKind);
    base->providerFcts.getLocation = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, ocrLocation_t*), ptrGetLocation);
    base->providerFcts.registerGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrGuid_t, u64), ptrRegisterGuid);
    base->providerFcts.releaseGuid = FUNC_ADDR(u8 (*)(ocrGuidProvider_t*, ocrFatGuid_t, bool), ptrReleaseGuid);
    return base;
}

#endif /* ENABLE_GUID_PTR */
