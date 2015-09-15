/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_WST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/wst/wst-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-WST SCHEDULER_OBJECT FUNCTIONS                 */
/******************************************************/

static void wstSchedulerObjectInit(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, u32 numDeques) {
    u32 i;
    ASSERT(numDeques > 0);
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self;
    wstSchedObj->numDeques = numDeques;
    wstSchedObj->deques = (ocrSchedulerObject_t**)pd->fcts.pdMalloc(pd, numDeques * sizeof(ocrSchedulerObject_t*));
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    //Instantiate the deque schedulerObjects
    paramListSchedulerObjectDeq_t params;
    params.base.kind = OCR_SCHEDULER_OBJECT_DEQUE | OCR_SCHEDULER_OBJECT_AGGREGATE;
    params.base.guidRequired = 0;
    params.type = WORK_STEALING_DEQUE;
    ocrSchedulerObjectFactory_t *dequeFactory = PD->schedulerObjectFactories[schedulerObjectDeq_id];
    for (i = 0; i < numDeques; i++) {
        wstSchedObj->deques[i] = dequeFactory->fcts.create(dequeFactory, (ocrParamList_t*)(&params));
    }
#else
    ASSERT(0);
#endif
}

static void wstSchedulerObjectFinish(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD) {
    u32 i;
    ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self;
    for (i = 0; i < wstSchedObj->numDeques; i++) {
        ocrSchedulerObject_t *deque = wstSchedObj->deques[i];
        ocrSchedulerObjectFactory_t *dequeFactory = PD->schedulerObjectFactories[deque->fctId];
        dequeFactory->fcts.destroy(dequeFactory, wstSchedObj->deques[i]);
    }
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    pd->fcts.pdFree(pd, wstSchedObj->deques);
}

ocrSchedulerObject_t* wstSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(SCHEDULER_OBJECT_KIND(paramSchedObj->kind) == OCR_SCHEDULER_OBJECT_WST);
    ASSERT(!paramSchedObj->guidRequired);
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectWst_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = paramSchedObj->kind;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    paramListSchedulerObjectWst_t *paramsWst = (paramListSchedulerObjectWst_t*)params;
    wstSchedulerObjectInit(schedObj, pd, paramsWst->numDeques);
    return schedObj;
}

u8 wstSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_WST);
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    wstSchedulerObjectFinish(self, pd);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 wstSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 wstSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 wstSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 wstSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    u64 count = 0;
    u32 i;
    ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self;
    ocrPolicyDomain_t *pd = fact->pd;
    for (i = 0; i < wstSchedObj->numDeques; i++) {
        ocrSchedulerObject_t *deque = wstSchedObj->deques[i];
        ocrSchedulerObjectFactory_t *dequeFactory = pd->schedulerObjectFactories[deque->fctId];
        count += dequeFactory->fcts.count(dequeFactory, deque, properties);
    }
    return count;
}

ocrSchedulerObject_t* wstGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    u32 i;
    ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self;
    for (i = 0; i < wstSchedObj->numDeques; i++) {
        if (wstSchedObj->deques[i]->loc == loc && wstSchedObj->deques[i]->mapping == mapping)
            return wstSchedObj->deques[i];
    }
    return NULL;
}

u8 wstSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* WST SCHEDULER_OBJECT ROOT FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectWst(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

ocrSchedulerObject_t* newSchedulerObjectRootWst(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectWst_t), PERSISTENT_CHUNK);
    schedObj->guid.guid = NULL_GUID; //Root objects are always static
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = factory->kind;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;

    ocrSchedulerObjectWst_t* wstSchedObj = (ocrSchedulerObjectWst_t*)schedObj;
    wstSchedObj->numDeques = 0;
    wstSchedObj->deques = NULL;

    return schedObj;
}

u8 wstSchedulerObjectSwitchRunlevel(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        break;
    case RL_MEMORY_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            u32 i;
            // The scheduler calls this before switching itself. Do we want
            // to invert this?
            for(i = 0; i < PD->schedulerObjectFactoryCount; ++i) {
                if(PD->schedulerObjectFactories[i])
                    PD->schedulerObjectFactories[i]->pd = PD;
            }
        }
        break;
    case RL_GUID_OK:
        // Memory is up
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
                ocrScheduler_t *scheduler = PD->schedulers[0];
                ocrSchedulerHeuristic_t *masterSchedulerHeuristic = scheduler->schedulerHeuristics[scheduler->masterHeuristicId];
                wstSchedulerObjectInit(self, PD, masterSchedulerHeuristic->contextCount);
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
                wstSchedulerObjectFinish(self, PD);
            }
        }
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_GUID_OK, phase)) {
                u32 i, w;
                ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self;
                ocrScheduler_t *scheduler = PD->schedulers[0];
                ocrSchedulerHeuristic_t *masterSchedulerHeuristic = scheduler->schedulerHeuristics[scheduler->masterHeuristicId];
                for (i = 0, w = 0; i < masterSchedulerHeuristic->contextCount; i++) {
                    ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)masterSchedulerHeuristic->contexts[i];
                    ocrSchedulerObject_t *deque = wstSchedObj->deques[i];
                    ocrSchedulerObjectFactory_t *dequeFactory = PD->schedulerObjectFactories[deque->fctId];
                    if (context->location == PD->myLocation) {
                        dequeFactory->fcts.setLocationForSchedulerObject(dequeFactory, deque, w++, OCR_SCHEDULER_OBJECT_MAPPING_WORKER);
                    } else {
                        dequeFactory->fcts.setLocationForSchedulerObject(dequeFactory, deque, context->location, OCR_SCHEDULER_OBJECT_MAPPING_PINNED);
                    }
                }
            }
        }
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
       ASSERT(wstSchedulerObjectCount(fact, schedObj, 0) == 0);
    */
}

void wstSchedulerObjectDestruct(ocrSchedulerObject_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

ocrSchedulerObjectActionSet_t* wstSchedulerObjectNewActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, u32 count) {
    ASSERT(0);
    return NULL;
}

void wstSchedulerObjectDestroyActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t *actionSet) {
    ASSERT(0);
    return;
}

/******************************************************/
/* OCR-WST SCHEDULER_OBJECT FACTORY FUNCTIONS         */
/******************************************************/

void destructSchedulerObjectRootFactoryWst(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryWst(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ocrSchedulerObjectFactory_t* schedObjFact = NULL;
    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectRootFactoryWst_t), PERSISTENT_CHUNK);
    } else {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryWst_t), PERSISTENT_CHUNK);
    }

    schedObjFact->factoryId = schedulerObjectWst_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_WST;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectWst;
    schedObjFact->destruct = &destructSchedulerObjectRootFactoryWst;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), wstSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), wstSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), wstSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), wstSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), wstGetSchedulerObjectForLocation);

    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact->kind |= OCR_SCHEDULER_OBJECT_ROOT;
        schedObjFact->instantiate = &newSchedulerObjectRootWst;
        ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
        rootFactory->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), wstSchedulerObjectSwitchRunlevel);
        rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObject_t*), wstSchedulerObjectDestruct);

        rootFactory->fcts.newActionSet = FUNC_ADDR(ocrSchedulerObjectActionSet_t* (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, u32), wstSchedulerObjectNewActionSet);
        rootFactory->fcts.destroyActionSet = FUNC_ADDR(void (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t*), wstSchedulerObjectDestroyActionSet);
    }

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_WST */
