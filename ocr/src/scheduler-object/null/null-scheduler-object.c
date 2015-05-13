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
/* OCR-NULL SCHEDULER_OBJECT FUNCTIONS                        */
/******************************************************/

ocrSchedulerObject_t* nullSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    ASSERT(0);
    return NULL;
}

u8 nullSchedulerObjectDestruct(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
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
/* NULL SCHEDULER_OBJECT ROOT FUNCTIONS                      */
/******************************************************/

void nullSchedulerObjectRootBegin(ocrSchedulerObjectRoot_t * self) {
    ASSERT(self->scheduler);
    return;
}

void nullSchedulerObjectRootStart(ocrSchedulerObjectRoot_t * self) {
    return;
}

void nullSchedulerObjectRootStop(ocrSchedulerObjectRoot_t * self) {
    return;
}

void nullSchedulerObjectRootFinish(ocrSchedulerObjectRoot_t * self) {
    return;
}

void nullSchedulerObjectRootDestruct(ocrSchedulerObjectRoot_t *self) {
    runtimeChunkFree((u64)self, NULL);
    return;
}

/******************************************************/
/* OCR-NULL SCHEDULER_OBJECT FACTORY FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectRootNull(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectRootNull_t), PERSISTENT_CHUNK);
    schedObj->guid = NULL_GUID;
    schedObj->kind = OCR_SCHEDULER_OBJECT_UNDEFINED;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;
    ocrSchedulerObjectRoot_t *rootSchedObj = (ocrSchedulerObjectRoot_t*)schedObj;
    ocrSchedulerObjectRootFactory_t *rootFact = (ocrSchedulerObjectRootFactory_t*)factory;
    rootSchedObj->scheduler = NULL;
    rootSchedObj->fcts = rootFact->fcts;
    return schedObj;
}

void destructSchedulerObjectRootFactoryNull(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
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
    schedObjFact->fcts.destruct = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), nullSchedulerObjectDestruct);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectRemove);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), nullSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), nullSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), nullGetSchedulerObjectForLocation);

    ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
    rootFactory->fcts.begin = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), nullSchedulerObjectRootBegin);
    rootFactory->fcts.start = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), nullSchedulerObjectRootStart);
    rootFactory->fcts.stop = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), nullSchedulerObjectRootStop);
    rootFactory->fcts.finish = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), nullSchedulerObjectRootFinish);
    rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), nullSchedulerObjectRootDestruct);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_NULL */
