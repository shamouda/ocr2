/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/deq/deq-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-DEQ SCHEDULER_OBJECT FUNCTIONS                 */
/******************************************************/

ocrSchedulerObject_t* deqSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(SCHEDULER_OBJECT_KIND(paramSchedObj->kind) == OCR_SCHEDULER_OBJECT_DEQUE);
    ASSERT(!paramSchedObj->guidRequired);
    paramListSchedulerObjectDeq_t *paramDeq = (paramListSchedulerObjectDeq_t*)params;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectDeq_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = paramSchedObj->kind;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectDeq_t* deqSchedObj = (ocrSchedulerObjectDeq_t*)schedObj;
    //deqSchedObj->deque = newDeque(pd, NULL, paramDeq->type);
    deqSchedObj->dequeType = paramDeq->type;
#ifdef SAL_FSIM_CE //TODO: This needs to be removed when bug #802 is fixed
    deqSchedObj->deque = NULL;
#else
    deqSchedObj->deque = newDeque(pd, NULL, paramDeq->type);
#endif
    return schedObj;
}

u8 deqSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_DEQUE);
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrSchedulerObjectDeq_t* deqSchedObj = (ocrSchedulerObjectDeq_t*)self;
    if (deqSchedObj->deque) deqSchedObj->deque->destruct(pd, deqSchedObj->deque);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 deqSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    ASSERT(IS_SCHEDULER_OBJECT_TYPE_SINGLETON(element->kind));
    deque_t * deq = schedObj->deque;
    if (deq == NULL) {
        ocrPolicyDomain_t *pd = NULL;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        deq = newDeque(pd, NULL, schedObj->dequeType);
        schedObj->deque = deq;
    }
    deq->pushAtTail(deq, (void *)(element->guid.guid), 0);
    return 0;
}

u8 deqSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    u32 i;
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    ASSERT(IS_SCHEDULER_OBJECT_TYPE_SINGLETON(kind));
    deque_t * deq = schedObj->deque;
    if (deq == NULL) return count;

    for (i = 0; i < count; i++) {
        ocrGuid_t retGuid = NULL_GUID;
        switch(properties) {
        case SCHEDULER_OBJECT_REMOVE_DEQ_POP:
            {
                START_PROFILE(sched_deq_Pop);
                retGuid = (ocrGuid_t)deq->popFromTail(deq, 0);
                EXIT_PROFILE;
            }
            break;
        case SCHEDULER_OBJECT_REMOVE_DEQ_STEAL:
            {
                START_PROFILE(sched_deq_Steal);
                retGuid = (ocrGuid_t)deq->popFromHead(deq, 1);
                EXIT_PROFILE;
            }
            break;
        default:
            ASSERT(0);
            return OCR_ENOTSUP;
        }

        if (retGuid == NULL_GUID)
            break;

        if (IS_SCHEDULER_OBJECT_TYPE_SINGLETON(dst->kind)) {
            ASSERT(dst->guid.guid == NULL_GUID && count == 1);
            dst->guid.guid = retGuid;
        } else {
            ocrSchedulerObject_t taken;
            taken.guid.guid = retGuid;
            taken.kind = kind;
            ocrSchedulerObjectFactory_t *dstFactory = fact->pd->schedulerObjectFactories[dst->fctId];
            dstFactory->fcts.insert(dstFactory, dst, &taken, 0);
        }
    }

    // Success (0) if at least one element has been removed
    return (i == 0);
}

u8 deqSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 deqSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    deque_t * deq = schedObj->deque;
    if (deq == NULL) return 0;
    return (deq->tail - deq->head); //this may be racy but ok for approx count
}

ocrSchedulerObject_t* deqGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(self->loc == loc && self->mapping == mapping);
    return self;
}

u8 deqSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-DEQ SCHEDULER_OBJECT FACTORY FUNCTIONS         */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectDeq(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryDeq(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDeq(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ASSERT(paramFact->kind == OCR_SCHEDULER_OBJECT_AGGREGATE);
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryDeq_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectDeq_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_DEQUE;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectDeq;
    schedObjFact->destruct = &destructSchedulerObjectFactoryDeq;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), deqSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), deqSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), deqSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), deqSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), deqGetSchedulerObjectForLocation);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_DEQ */
