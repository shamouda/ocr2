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
/* OCR-DEQ SCHEDULER_OBJECT FUNCTIONS                        */
/******************************************************/

ocrSchedulerObject_t* deqSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(paramSchedObj->kind == OCR_SCHEDULER_OBJECT_DEQUE);
    paramListSchedulerObjectDeq_t *paramDeq = (paramListSchedulerObjectDeq_t*)params;
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectDeq_t));
    schedObj->guid = NULL_GUID;
    schedObj->kind = OCR_SCHEDULER_OBJECT_DEQUE;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectDeq_t* deqSchedObj = (ocrSchedulerObjectDeq_t*)schedObj;
    deqSchedObj->deque = newDeque(pd, NULL, paramDeq->type);
    return schedObj;
}

u8 deqSchedulerObjectDestruct(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(self->kind == OCR_SCHEDULER_OBJECT_DEQUE);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObjectDeq_t* deqSchedObj = (ocrSchedulerObjectDeq_t*)self;
    deqSchedObj->deque->destruct(pd, deqSchedObj->deque);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 deqSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    switch(element->kind) {
    case OCR_SCHEDULER_OBJECT_EDT:
        {
            deque_t * deq = schedObj->deque;
            deq->pushAtTail(deq, (void *)(element->guid), 0);
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 deqSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    u32 i;
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    deque_t * deq;
    ASSERT(IS_SCHEDULER_OBJECT_SINGLETON_KIND(kind));
    switch(kind) {
    case OCR_SCHEDULER_OBJECT_EDT:
        deq = schedObj->deque;
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    ocrSchedulerObject_t taken;
    taken.kind = kind;

    ocrSchedulerObjectFactory_t *dstFactory = fact->pd->schedulerObjectFactories[dst->fctId];
    for (i = 0; i < count; i++) {
        taken.guid = NULL_GUID;
        switch(properties) {
        case SCHEDULER_OBJECT_REMOVE_DEQ_POP:
            {
                START_PROFILE(sched_deq_Pop);
                taken.guid = (ocrGuid_t)deq->popFromTail(deq, 0);
                EXIT_PROFILE;
            }
            break;
        case SCHEDULER_OBJECT_REMOVE_DEQ_STEAL:
            {
                START_PROFILE(sched_deq_Steal);
                taken.guid = (ocrGuid_t)deq->popFromHead(deq, 1);
                EXIT_PROFILE;
            }
            break;
        default:
            ASSERT(0);
            return OCR_ENOTSUP;
        }

        if (taken.guid == NULL_GUID)
            break;

        dstFactory->fcts.insert(dstFactory, dst, &taken, 0);
    }

    return (count - i);
}

u64 deqSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectDeq_t *schedObj = (ocrSchedulerObjectDeq_t*)self;
    deque_t * deq = schedObj->deque;
    return (deq->tail - deq->head); //this may be racy but ok for approx count
}

ocrSchedulerObject_t* deqGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 deqSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-DEQ SCHEDULER_OBJECT FACTORY FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectDeq(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryDeq(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectAggregateFactoryDeq(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryDeq_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectDeq_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_DEQUE;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectDeq;
    schedObjFact->destruct = &destructSchedulerObjectFactoryDeq;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), deqSchedulerObjectCreate);
    schedObjFact->fcts.destruct = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), deqSchedulerObjectDestruct);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectRemove);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), deqSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), deqSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), deqGetSchedulerObjectForLocation);

    return schedObjFact;
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDeq(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    switch(SCHEDULER_OBJECT_KIND_TYPE(paramFact->kind)) {
    case OCR_SCHEDULER_OBJECT_AGGREGATE:
        return newOcrSchedulerObjectAggregateFactoryDeq(perType, factoryId);
    default:
        ASSERT(0);
        break;
    }
    return NULL;
}

#endif /* ENABLE_SCHEDULER_OBJECT_DEQ */
