/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/null/null-scheduler-object.h"

/******************************************************/
/* OCR-NULL SCHEDULER_OBJECT FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* nullSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    ASSERT(0);
    return NULL;
}

u8 nullSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 nullSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

ocrSchedulerObject_t* nullGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 nullSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc) {
    return OCR_ENOTSUP;
}

/******************************************************/
/* NULL SCHEDULER_OBJECT ROOT FUNCTIONS               */
/******************************************************/

u8 nullSchedulerObjectSwitchRunlevel(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
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

void nullSchedulerObjectDestruct(ocrSchedulerObject_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
    return;
}

ocrSchedulerObjectActionSet_t* nullSchedulerObjectNewActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, u32 count) {
    ASSERT(0);
    return NULL;
}

void nullSchedulerObjectDestroyActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t *actionSet) {
    ASSERT(0);
    return;
}

/******************************************************/
/* OCR-NULL SCHEDULER_OBJECT FACTORY FUNCTIONS        */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectRootNull(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectNull_t), PERSISTENT_CHUNK);
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = OCR_SCHEDULER_OBJECT_UNDEFINED;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;
    return schedObj;
}

void destructSchedulerObjectRootFactoryNull(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryNull(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectRootFactoryNull_t), NONPERSISTENT_CHUNK);

    schedObjFact->factoryId = factoryId;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_UNDEFINED;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectRootNull;
    schedObjFact->destruct = &destructSchedulerObjectRootFactoryNull;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), nullSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), nullSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), nullSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), nullSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), nullGetSchedulerObjectForLocation);

    ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
    rootFactory->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), nullSchedulerObjectSwitchRunlevel);
    rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObject_t*), nullSchedulerObjectDestruct);
    rootFactory->fcts.newActionSet = FUNC_ADDR(ocrSchedulerObjectActionSet_t* (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, u32), nullSchedulerObjectNewActionSet);
    rootFactory->fcts.destroyActionSet = FUNC_ADDR(void (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrSchedulerObjectActionSet_t*), nullSchedulerObjectDestroyActionSet);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_NULL */
