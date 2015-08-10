/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#include "extensions/ocr-hints.h"
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/bin-heap/bin-heap-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-BIN_HEAP SCHEDULER_OBJECT FUNCTIONS            */
/******************************************************/

ocrSchedulerObject_t* binHeapSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(SCHEDULER_OBJECT_KIND(paramSchedObj->kind) == OCR_SCHEDULER_OBJECT_BIN_HEAP);
    ASSERT(!paramSchedObj->guidRequired);
    paramListSchedulerObjectBinHeap_t *paramBinHeap = (paramListSchedulerObjectBinHeap_t*)params;
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectBinHeap_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = paramSchedObj->kind;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectBinHeap_t* binHeapSchedObj = (ocrSchedulerObjectBinHeap_t*)schedObj;
    binHeapSchedObj->binHeap = newBinHeap(pd, paramBinHeap->type);
    return schedObj;
}

u8 binHeapSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_BIN_HEAP);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObjectBinHeap_t* binHeapSchedObj = (ocrSchedulerObjectBinHeap_t*)self;
    binHeapSchedObj->binHeap->destruct(pd, binHeapSchedObj->binHeap);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 binHeapSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ocrSchedulerObjectBinHeap_t *schedObj = (ocrSchedulerObjectBinHeap_t*)self;
    ASSERT(IS_SCHEDULER_OBJECT_TYPE_SINGLETON(element->kind));
    binHeap_t * heap = schedObj->binHeap;
    ocrGuid_t edtGuid = element->guid.guid;
    s64 priority;
    { // read EDT hint
        ASSERT(element->kind == OCR_SCHEDULER_OBJECT_EDT);
        ocrHint_t edtHints;
        ocrHintInit(&edtHints, OCR_HINT_EDT_T);
        ocrGetHint(edtGuid, &edtHints);
        ocrGetHintValue(&edtHints, OCR_HINT_EDT_PRIORITY, (u64*)&priority);
    }
    heap->push(heap, (void *)edtGuid, priority, 0);
    return 0;
}

u8 binHeapSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    u32 i;
    ocrSchedulerObjectBinHeap_t *schedObj = (ocrSchedulerObjectBinHeap_t*)self;
    ASSERT(IS_SCHEDULER_OBJECT_TYPE_SINGLETON(kind));
    binHeap_t * heap = schedObj->binHeap;

    for (i = 0; i < count; i++) {
        ocrGuid_t retGuid = NULL_GUID;

        START_PROFILE(sched_heap_Pop);
        retGuid = (ocrGuid_t)heap->pop(heap, 0);
        EXIT_PROFILE;

        if (retGuid == NULL_GUID) break;

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

u8 binHeapSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 binHeapSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectBinHeap_t *schedObj = (ocrSchedulerObjectBinHeap_t*)self;
    binHeap_t * heap = schedObj->binHeap;
    return heap->count; //this may be racy but ok for approx count
}

ocrSchedulerObject_t* heapGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(self->loc == loc && self->mapping == mapping);
    return self;
}

u8 binHeapSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-BIN_HEAP SCHEDULER_OBJECT FACTORY FUNCTIONS    */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectBinHeap(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryBinHeap(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryBinHeap(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ASSERT(paramFact->kind == OCR_SCHEDULER_OBJECT_AGGREGATE);
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryBinHeap_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectBinHeap_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_BIN_HEAP;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectBinHeap;
    schedObjFact->destruct = &destructSchedulerObjectFactoryBinHeap;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), binHeapSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), binHeapSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), binHeapSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), binHeapSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), binHeapSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), binHeapSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), binHeapSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), heapGetSchedulerObjectForLocation);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_BIN_HEAP */
