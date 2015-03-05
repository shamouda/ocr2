/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 * - A scheduler heuristic for WST root schedulerObjects
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "scheduler-heuristic/hc/hc-scheduler-heuristic.h"
#include "scheduler-object/wst/wst-scheduler-object.h"

/******************************************************/
/* OCR-HC SCHEDULER_HEURISTIC                                 */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicHc(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicHc_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    return self;
}

void hcSchedulerHeuristicBegin(ocrSchedulerHeuristic_t * self) {
    ASSERT(self->scheduler != NULL);
    ASSERT(self->scheduler->rootObj->base.kind == OCR_SCHEDULER_OBJECT_ROOT_WST);
}

void initializeContext(ocrSchedulerHeuristicContext_t *context, u64 contextId) {
    context->id = contextId;
    context->actionSet = NULL;
    context->cost = NULL;
    context->properties = 0;

    ocrSchedulerHeuristicContextHc_t *hcContext = (ocrSchedulerHeuristicContextHc_t*)context;
    hcContext->mySchedulerObjectIndex = ((u64)-1);
    hcContext->stealSchedulerObjectIndex = ((u64)-1);
    hcContext->mySchedulerObject = NULL;
    return;
}

void hcSchedulerHeuristicStart(ocrSchedulerHeuristic_t * self) {
    u32 i;
    ocrScheduler_t *scheduler = self->scheduler;
    ASSERT(scheduler->pd != NULL);
    ASSERT(scheduler->contextCount > 0);
    ocrPolicyDomain_t *pd = scheduler->pd;
    self->contextCount = scheduler->contextCount;
    self->contexts = (ocrSchedulerHeuristicContext_t **)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContext_t*));
    ocrSchedulerHeuristicContextHc_t *contextAlloc = (ocrSchedulerHeuristicContextHc_t *)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContextHc_t));
    for (i = 0; i < self->contextCount; i++) {
        ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)&(contextAlloc[i]);
        initializeContext(context, i);
        self->contexts[i] = context;
    }
}

void hcSchedulerHeuristicStop(ocrSchedulerHeuristic_t * self) {
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    ocrSchedulerHeuristicContextHc_t *contextAlloc = (ocrSchedulerHeuristicContextHc_t *)self->contexts[0];
    pd->fcts.pdFree(pd, contextAlloc);
}

void hcSchedulerHeuristicFinish(ocrSchedulerHeuristic_t *self) {
    // Nothing to do locally
}

void hcSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 hcSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, ocrSchedulerObject_t *schedulerObject, u32 properties) {
    return OCR_ENOTSUP;
}

ocrSchedulerHeuristicContext_t* hcSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, u64 contextId) {
    return self->contexts[contextId];
}

u8 hcSchedulerHeuristicRegisterContext(ocrSchedulerHeuristic_t *self, u64 contextId, ocrLocation_t loc) {
    ocrSchedulerHeuristicContext_t *context = self->contexts[contextId];
    ocrSchedulerHeuristicContextHc_t *hcContext = (ocrSchedulerHeuristicContextHc_t*)context;
    ocrSchedulerObjectRootWst_t *wstSchedObj = (ocrSchedulerObjectRootWst_t*)self->scheduler->rootObj;
    ASSERT(contextId < wstSchedObj->numDeques);
    hcContext->mySchedulerObject = wstSchedObj->deques[contextId];
    ASSERT(hcContext->mySchedulerObject);
    hcContext->mySchedulerObjectIndex = contextId;
    hcContext->stealSchedulerObjectIndex = (hcContext->mySchedulerObjectIndex + 1) % wstSchedObj->numDeques;
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[hcContext->mySchedulerObject->fctId];
    fact->fcts.setLocationForSchedulerObject(fact, hcContext->mySchedulerObject, loc, OCR_SCHEDULER_OBJECT_MAPPING_PINNED);
    return 0;
}

u8 hcSchedulerHeuristicGiveInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContextHc_t *hcContext = (ocrSchedulerHeuristicContextHc_t*)context;
    ocrSchedulerObject_t *schedObj = hcContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    ASSERT(fact->fcts.insert(fact, schedObj, opArgs->el, 0) == 0);
    return 0;
}

u8 hcSchedulerHeuristicTakeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContextHc_t *hcContext = (ocrSchedulerHeuristicContextHc_t*)context;
    ocrSchedulerObject_t *schedObj = hcContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    u8 retVal = fact->fcts.remove(fact, schedObj, opArgs->takeKind, opArgs->takeCount, opArgs->el, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_POP);

    if (retVal != 0) {
        ocrSchedulerObjectRoot_t *rootObj = self->scheduler->rootObj;
        ocrSchedulerObjectRootWst_t *wstSchedObj = (ocrSchedulerObjectRootWst_t*)rootObj;
        ocrSchedulerObject_t *stealSchedulerObject = wstSchedObj->deques[hcContext->stealSchedulerObjectIndex];
        ASSERT(stealSchedulerObject->fctId == schedObj->fctId);
        retVal = fact->fcts.remove(fact, stealSchedulerObject, opArgs->takeKind, opArgs->takeCount, opArgs->el, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_STEAL); //try cached value first

        ocrSchedulerObject_t *sSchedObj = (ocrSchedulerObject_t*)rootObj;
        ocrSchedulerObjectFactory_t *sFact = self->scheduler->pd->schedulerObjectFactories[sSchedObj->fctId];
        while (retVal != 0 && sFact->fcts.count(sFact, (ocrSchedulerObject_t*)wstSchedObj, SCHEDULER_OBJECT_COUNT_NONEMPTY | SCHEDULER_OBJECT_COUNT_RECURSIVE | SCHEDULER_OBJECT_COUNT_EDT) != 0) {
            u32 i;
            for (i = 1; retVal != 0 && i < wstSchedObj->numDeques; i++) {
                hcContext->stealSchedulerObjectIndex = (hcContext->mySchedulerObjectIndex + i) % wstSchedObj->numDeques;
                stealSchedulerObject = wstSchedObj->deques[hcContext->stealSchedulerObjectIndex];
                ASSERT(stealSchedulerObject->fctId == schedObj->fctId);
                retVal = fact->fcts.remove(fact, stealSchedulerObject, opArgs->takeKind, opArgs->takeCount, opArgs->el, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_STEAL);
            }
        }
    }

    return retVal;
}

u8 hcSchedulerHeuristicGiveSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
#if 0 //Example simulate op
    ASSERT(opArgs->schedulerObject->kind == OCR_SCHEDULER_OBJECT_EDT);
    ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t*)self->contexts[opArgs->contextId];
    ocrSchedulerHeuristicContextHc_t *hcContext = (ocrSchedulerHeuristicContextHc_t*)context;
    ASSERT(context->actionSet == NULL);

    ocrSchedulerObjectActionSet_t *actionSet = &(hcContext->singleActionSet);
    actionSet->actions = &(hcContext->insertAction);
    ASSERT(actionSet->actions->schedulerObject == hcContext->mySchedulerObject);
    actionSet->actions->args.insert.el = opArgs->schedulerObject;
    context->actionSet = actionSet;
#endif
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 hcSchedulerHeuristicTakeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-HC SCHEDULER_HEURISTIC FACTORY                         */
/******************************************************/

void destructSchedulerHeuristicFactoryHc(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryHc(ocrParamList_t *perType) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactoryHc_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newSchedulerHeuristicHc;
    base->destruct = &destructSchedulerHeuristicFactoryHc;
    base->fcts.begin = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hcSchedulerHeuristicBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hcSchedulerHeuristicStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hcSchedulerHeuristicStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hcSchedulerHeuristicFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hcSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), hcSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, u64), hcSchedulerHeuristicGetContext);
    base->fcts.registerContext = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u64, ocrLocation_t), hcSchedulerHeuristicRegisterContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GIVE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerHeuristicGiveInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TAKE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerHeuristicTakeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GIVE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerHeuristicGiveSimulate);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TAKE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerHeuristicTakeSimulate);
    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_HC */