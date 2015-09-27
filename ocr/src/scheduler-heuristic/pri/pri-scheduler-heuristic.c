/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 * - A scheduler heuristic for WST root schedulerObjects
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_PRI

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "scheduler-heuristic/pri/pri-scheduler-heuristic.h"
#include "scheduler-object/wst/wst-scheduler-object.h"
#include "scheduler-object/plist/plist-scheduler-object.h"
#include "extensions/ocr-hints.h"
#include "task/hc/hc-task.h"

/******************************************************/
/* OCR-PRI SCHEDULER_HEURISTIC                         */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicPri(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicPri_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    return self;
}

static void initializeContextPri(ocrSchedulerHeuristicContext_t *context, u64 contextId) {
    context->id = contextId;
    context->actionSet = NULL;
    context->cost = NULL;
    context->properties = 0;

    ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)context;
    priContext->stealSchedulerObjectIndex = ((u64)-1);
    priContext->mySchedulerObject = NULL;
    return;
}

u8 priSchedulerHeuristicSwitchRunlevel(ocrSchedulerHeuristic_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
    {
        ocrScheduler_t *scheduler = self->scheduler;
        ASSERT(scheduler);
        self->contextCount = PD->workerCount; //Shared mem heuristic
        ASSERT(self->contextCount > 0);
        break;
    }
    case RL_MEMORY_OK:
    {
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
            u32 i;
            self->contexts = (ocrSchedulerHeuristicContext_t **)PD->fcts.pdMalloc(PD, self->contextCount * sizeof(ocrSchedulerHeuristicContext_t*));
            ocrSchedulerHeuristicContextPri_t *contextAlloc = (ocrSchedulerHeuristicContextPri_t *)PD->fcts.pdMalloc(PD, self->contextCount * sizeof(ocrSchedulerHeuristicContextPri_t));
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)&(contextAlloc[i]);
                initializeContextPri(context, i);
                self->contexts[i] = context;
                context->id = i;
                context->location = PD->myLocation;
                context->actionSet = NULL;
                context->cost = NULL;
                context->properties = 0;
                ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)context;
                priContext->stealSchedulerObjectIndex = ((u64)-1);
                priContext->mySchedulerObject = NULL;
            }
        }
        if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
            PD->fcts.pdFree(PD, self->contexts[0]);
            PD->fcts.pdFree(PD, self->contexts);
        }
        break;
    }
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
    {
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
            u32 i;
            ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
            ocrSchedulerObjectFactory_t *rootFact = PD->schedulerObjectFactories[rootObj->fctId];
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)self->contexts[i];
                priContext->mySchedulerObject = rootFact->fcts.getSchedulerObjectForLocation(rootFact, rootObj, i, OCR_SCHEDULER_OBJECT_MAPPING_WORKER, 0);
                ASSERT(priContext->mySchedulerObject);
                priContext->stealSchedulerObjectIndex = (i + 1) % self->contextCount;
            }
        }
        break;
    }
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

void priSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 priSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, u32 properties) {
    return OCR_ENOTSUP;
}

ocrSchedulerHeuristicContext_t* priSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, ocrLocation_t loc) {
    ASSERT(loc == self->scheduler->pd->myLocation);
    ocrWorker_t * worker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    return self->contexts[worker->id];
}

/* Find EDT for the worker to execute - This uses random workstealing to find work if no work is found owned deque */
static u8 priSchedulerHeuristicWorkEdtUserInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    ocrSchedulerObject_t edtObj;
    edtObj.guid.guid = NULL_GUID;
    edtObj.guid.metaDataPtr = NULL;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;

    //If priority list contains elements, then pick them first
    ocrSchedulerObjectWst_t *wstObj = (ocrSchedulerObjectWst_t*)self->scheduler->rootObj;
    ocrSchedulerObjectPlist_t *plist = (ocrSchedulerObjectPlist_t*)wstObj->plist;
    ocrSchedulerObjectFactory_t *pfact = self->scheduler->pd->schedulerObjectFactories[wstObj->plist->fctId];
    if (pfact->fcts.count(pfact, wstObj->plist, 0) != 0) {
        hal_lock32(&plist->lock);
        if (pfact->fcts.count(pfact, wstObj->plist, 0) != 0) {
            ASSERT(pfact->fcts.remove(pfact, wstObj->plist, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_TAIL) == 0);
            if (edtObj.guid.guid != NULL_GUID)
                taskArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt = edtObj.guid;
            hal_unlock32(&plist->lock);
            return 0;
        }
        hal_unlock32(&plist->lock);
    }

    //Try to pop from own deque
    ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)context;
    ocrSchedulerObject_t *schedObj = priContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    u8 retVal = fact->fcts.remove(fact, schedObj, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_TAIL);

    //If pop fails, then try to steal from other deques
    if (edtObj.guid.guid == NULL_GUID) {
        //First try to steal from the last deque that was visited (probably had a successful steal)
        ocrSchedulerObject_t *stealSchedulerObject = ((ocrSchedulerHeuristicContextPri_t*)self->contexts[priContext->stealSchedulerObjectIndex])->mySchedulerObject;
        ASSERT(stealSchedulerObject);
        retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_HEAD); //try cached deque first

        //If cached steal failed, then restart steal loop from starting index
        ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
        ocrSchedulerObjectFactory_t *sFact = self->scheduler->pd->schedulerObjectFactories[rootObj->fctId];
        while (edtObj.guid.guid == NULL_GUID && sFact->fcts.count(sFact, rootObj, (SCHEDULER_OBJECT_COUNT_EDT | SCHEDULER_OBJECT_COUNT_RECURSIVE) ) != 0) {
            u32 i;
            for (i = 1; edtObj.guid.guid == NULL_GUID && i < self->contextCount; i++) {
                priContext->stealSchedulerObjectIndex = (context->id + i) % self->contextCount; //simple round robin stealing
                stealSchedulerObject = ((ocrSchedulerHeuristicContextPri_t*)self->contexts[priContext->stealSchedulerObjectIndex])->mySchedulerObject;
                if (stealSchedulerObject)
                    retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_HEAD);
            }
        }
    }

    if (edtObj.guid.guid != NULL_GUID)
        taskArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt = edtObj.guid;
    return retVal;
}

u8 priSchedulerHeuristicGetWorkInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContext_t *context = self->fcts.getContext(self, opArgs->location);
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    switch(taskArgs->kind) {
    case OCR_SCHED_WORK_EDT_USER:
        return priSchedulerHeuristicWorkEdtUserInvoke(self, context, opArgs, hints);
    // Unknown ops
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 priSchedulerHeuristicGetWorkSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

static u8 priSchedulerHeuristicNotifyEdtReadyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)context;
    ocrSchedulerObject_t *schedObj = priContext->mySchedulerObject;
    ASSERT(schedObj);

    ocrSchedulerObject_t edtObj;
    edtObj.guid = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;

    //Get the priority hint if set on the EDT
    u64 priority = 0;
    ocrTask_t *task = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid.metaDataPtr;
    ASSERT(task);
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrGetHint(task->guid, &edtHint);
    if (ocrGetHintValue(&edtHint, OCR_HINT_EDT_PRIORITY, &priority) == 0) {
        if (priority != 0) {
            ocrSchedulerObjectWst_t *wstObj = (ocrSchedulerObjectWst_t*)self->scheduler->rootObj;
            ocrSchedulerObjectPlist_t *plist = (ocrSchedulerObjectPlist_t*)wstObj->plist;
            ocrSchedulerObjectFactory_t *pfact = self->scheduler->pd->schedulerObjectFactories[wstObj->plist->fctId];
            hal_lock32(&plist->lock);
            wstObj->iterator->data = (void*)priority;
            pfact->fcts.insert(pfact, wstObj->plist, &edtObj, wstObj->iterator, SCHEDULER_OBJECT_INSERT_POSITION_ITERATOR);
            hal_unlock32(&plist->lock);
            return 0;
        }
    }
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    return fact->fcts.insert(fact, schedObj, &edtObj, NULL, (SCHEDULER_OBJECT_INSERT_AFTER | SCHEDULER_OBJECT_INSERT_POSITION_TAIL));
}

u8 priSchedulerHeuristicNotifyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContext_t *context = self->fcts.getContext(self, opArgs->location);
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    switch(notifyArgs->kind) {
    case OCR_SCHED_NOTIFY_EDT_READY:
        return priSchedulerHeuristicNotifyEdtReadyInvoke(self, context, opArgs, hints);
    case OCR_SCHED_NOTIFY_EDT_DONE:
        {
            // Destroy the work
            ocrPolicyDomain_t *pd;
            PD_MSG_STACK(msg);
            getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_DESTROY
            getCurrentEnv(NULL, NULL, NULL, &msg);
            msg.type = PD_MSG_WORK_DESTROY | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_DONE).guid;
            PD_MSG_FIELD_I(currentEdt) = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_DONE).guid;
            PD_MSG_FIELD_I(properties) = 0;
            ASSERT(pd->fcts.processMessage(pd, &msg, false) == 0);
#undef PD_MSG
#undef PD_TYPE
        }
        break;
    // Notifies ignored by this heuristic
    case OCR_SCHED_NOTIFY_EDT_SATISFIED:
    case OCR_SCHED_NOTIFY_DB_CREATE:
        return OCR_ENOP;
    // Unknown ops
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

#if 0 //Example simulate op
u8 priSchedulerHeuristicGiveSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(opArgs->schedulerObject->kind == OCR_SCHEDULER_OBJECT_EDT);
    ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t*)self->contexts[opArgs->contextId];
    ocrSchedulerHeuristicContextPri_t *priContext = (ocrSchedulerHeuristicContextPri_t*)context;
    ASSERT(context->actionSet == NULL);

    ocrSchedulerObjectActionSet_t *actionSet = &(priContext->singleActionSet);
    actionSet->actions = &(priContext->insertAction);
    ASSERT(actionSet->actions->schedulerObject == priContext->mySchedulerObject);
    actionSet->actions->args.insert.el = opArgs->schedulerObject;
    context->actionSet = actionSet;
    ASSERT(0);
    return OCR_ENOTSUP;
}
#endif

u8 priSchedulerHeuristicNotifySimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 priSchedulerHeuristicTransactInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 priSchedulerHeuristicTransactSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 priSchedulerHeuristicAnalyzeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 priSchedulerHeuristicAnalyzeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-PRI SCHEDULER_HEURISTIC FACTORY                 */
/******************************************************/

void destructSchedulerHeuristicFactoryPri(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryPri(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactoryPri_t), NONPERSISTENT_CHUNK);
    base->factoryId = factoryId;
    base->instantiate = &newSchedulerHeuristicPri;
    base->destruct = &destructSchedulerHeuristicFactoryPri;
    base->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                 phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), priSchedulerHeuristicSwitchRunlevel);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), priSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), priSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, ocrLocation_t), priSchedulerHeuristicGetContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicGetWorkInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicGetWorkSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicNotifyInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicNotifySimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicTransactInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicTransactSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicAnalyzeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), priSchedulerHeuristicAnalyzeSimulate);

    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_PRI */
