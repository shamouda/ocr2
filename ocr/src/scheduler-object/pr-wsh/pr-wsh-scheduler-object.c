/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/pr-wsh/pr-wsh-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-PR_WSH SCHEDULER_OBJECT FUNCTIONS              */
/******************************************************/

static void prWshSchedulerObjectInit(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD) {
    ocrScheduler_t *scheduler = PD->schedulers[0];
    ASSERT(scheduler);

    ocrSchedulerObjectPrWsh_t *prWshSchedObj = (ocrSchedulerObjectPrWsh_t*)self;
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    //Instantiate the binHeap schedulerObjects
    paramListSchedulerObjectBinHeap_t params;
    params.base.kind = OCR_SCHEDULER_OBJECT_BIN_HEAP | OCR_SCHEDULER_OBJECT_AGGREGATE;
    params.base.guidRequired = 0;
    params.type = LOCKED_BIN_HEAP;
    ocrSchedulerObjectFactory_t *binHeapFactory = PD->schedulerObjectFactories[schedulerObjectBinHeap_id];
    prWshSchedObj->heap = binHeapFactory->fcts.create(binHeapFactory, (ocrParamList_t*)(&params));
    binHeapFactory->fcts.setLocationForSchedulerObject(binHeapFactory, prWshSchedObj->heap, 0, OCR_SCHEDULER_OBJECT_MAPPING_PINNED);
#else
    ASSERT(0);
#endif
}

static void prWshSchedulerObjectFinish(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD) {
    ocrSchedulerObjectPrWsh_t *prWshSchedObj = (ocrSchedulerObjectPrWsh_t*)self;
    ocrSchedulerObject_t *binHeap = prWshSchedObj->heap;
    ocrSchedulerObjectFactory_t *binHeapFactory = PD->schedulerObjectFactories[binHeap->fctId];
    binHeapFactory->fcts.destroy(binHeapFactory, binHeap);
}

ocrSchedulerObject_t* prWshSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(SCHEDULER_OBJECT_KIND(paramSchedObj->kind) == OCR_SCHEDULER_OBJECT_PR_WSH);
    ASSERT(!paramSchedObj->guidRequired);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectPrWsh_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = paramSchedObj->kind;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    prWshSchedulerObjectInit(schedObj, pd);
    return schedObj;
}

u8 prWshSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_PR_WSH);
    ocrPolicyDomain_t *pd = fact->pd;
    prWshSchedulerObjectFinish(self, pd);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 prWshSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 prWshSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 prWshSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 prWshSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectPrWsh_t *prWshSchedObj = (ocrSchedulerObjectPrWsh_t*)self;
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t *binHeap = prWshSchedObj->heap;
    ocrSchedulerObjectFactory_t *binHeapFactory = pd->schedulerObjectFactories[binHeap->fctId];
    return binHeapFactory->fcts.count(binHeapFactory, binHeap, properties);
}

ocrSchedulerObject_t* prWshGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ocrSchedulerObjectPrWsh_t *prWshSchedObj = (ocrSchedulerObjectPrWsh_t*)self;
    return prWshSchedObj->heap;
}

u8 prWshSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* PR_WSH SCHEDULER_OBJECT ROOT FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectPrWsh(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

ocrSchedulerObject_t* newSchedulerObjectRootPrWsh(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectPrWsh_t), PERSISTENT_CHUNK);
    schedObj->guid.guid = NULL_GUID; //Root objects are always static
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = factory->kind;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;

    ocrSchedulerObjectPrWsh_t* prWshSchedObj = (ocrSchedulerObjectPrWsh_t*)schedObj;
    prWshSchedObj->heap = NULL;

    return schedObj;
}

u8 prWshSchedulerObjectSwitchRunlevel(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            u32 i;
            // The scheduler calls this before switching itself. Do we want
            // to invert this?
            for(i = 0; i < PD->schedulerObjectFactoryCount; ++i) {
                PD->schedulerObjectFactories[i]->pd = PD;
            }
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        // Memory is up
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_GUID_OK, phase)) {
                prWshSchedulerObjectInit(self, PD);
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
                prWshSchedulerObjectFinish(self, PD);
            }
        }
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        ASSERT(0);
    }
    return toReturn;
    /* BUG #583: There was this code on STOP, not sure if we still need it
       ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)self;
       ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
       ASSERT(prWshSchedulerObjectCount(fact, schedObj, 0) == 0);
    */
}

void prWshSchedulerObjectDestruct(ocrSchedulerObject_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

ocrSchedulerObjectActionSet_t* prWshSchedulerObjectNewActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, u32 count) {
    ASSERT(0);
    return NULL;
}

void prWshSchedulerObjectDestroyActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t *actionSet) {
    ASSERT(0);
    return;
}

/******************************************************/
/* OCR-PR_WSH SCHEDULER_OBJECT FACTORY FUNCTIONS         */
/******************************************************/

void destructSchedulerObjectRootFactoryPrWsh(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryPrWsh(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ocrSchedulerObjectFactory_t* schedObjFact = NULL;
    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectRootFactoryPrWsh_t), PERSISTENT_CHUNK);
    } else {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryPrWsh_t), PERSISTENT_CHUNK);
    }

    schedObjFact->factoryId = schedulerObjectPrWsh_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_PR_WSH;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectPrWsh;
    schedObjFact->destruct = &destructSchedulerObjectRootFactoryPrWsh;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), prWshSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), prWshSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), prWshSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), prWshSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), prWshSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), prWshSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), prWshSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), prWshGetSchedulerObjectForLocation);

    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact->kind |= OCR_SCHEDULER_OBJECT_ROOT;
        schedObjFact->instantiate = &newSchedulerObjectRootPrWsh;
        ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
        rootFactory->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), prWshSchedulerObjectSwitchRunlevel);
        rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObject_t*), prWshSchedulerObjectDestruct);

        rootFactory->fcts.newActionSet = FUNC_ADDR(ocrSchedulerObjectActionSet_t* (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, u32), prWshSchedulerObjectNewActionSet);
        rootFactory->fcts.destroyActionSet = FUNC_ADDR(void (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t*), prWshSchedulerObjectDestroyActionSet);
    }

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_PR_WSH */
