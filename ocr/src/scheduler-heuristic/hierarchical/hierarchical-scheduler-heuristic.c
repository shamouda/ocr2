/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 * - A scheduler heuristic for WST root schedulerObjects
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_HIERARCHICAL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "scheduler-heuristic/hierarchical/hierarchical-scheduler-heuristic.h"

/******************************************************/
/* OCR-HC SCHEDULER_HEURISTIC                         */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicHierarchical(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicHierarchical_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    return self;
}

static void initializeContext(ocrSchedulerHeuristicContext_t *context, u64 contextId) {
    context->id = contextId;
    context->actionSet = NULL;
    context->cost = NULL;
    context->properties = 0;

    ocrSchedulerHeuristicContextHierarchical_t *hcContext = (ocrSchedulerHeuristicContextHierarchical_t*)context;
    hcContext->stealSchedulerObjectIndex = ((u64)-1);
    hcContext->mySchedulerObject = NULL;
    return;
}

u8 hierarchicalSchedulerHeuristicSwitchRunlevel(ocrSchedulerHeuristic_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        break;
    case RL_GUID_OK:
    {
        // Memory is up at this point. We can initialize ourself
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_GUID_OK, phase)) {
            u32 i;
            ocrScheduler_t *scheduler = self->scheduler;
            ASSERT(scheduler);
            ASSERT(SCHEDULER_OBJECT_KIND(self->scheduler->rootObj->kind) == OCR_SCHEDULER_OBJECT_WST);
            ASSERT(scheduler->pd != NULL);
            ASSERT(scheduler->contextCount > 0);
            ocrPolicyDomain_t *pd = scheduler->pd;
            ASSERT(pd == PD);

            self->contextCount = scheduler->contextCount;
            self->contexts = (ocrSchedulerHeuristicContext_t **)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContext_t*));
            ocrSchedulerHeuristicContextHierarchical_t *contextAlloc = (ocrSchedulerHeuristicContextHierarchical_t *)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContextHierarchical_t));
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)&(contextAlloc[i]);
                initializeContext(context, i);
                self->contexts[i] = context;
            }
        }
        if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
            PD->fcts.pdFree(PD, self->contexts[0]);
            PD->fcts.pdFree(PD, self->contexts);
        }
        break;
    }
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

void hierarchicalSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 hierarchicalSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, ocrSchedulerObject_t *schedulerObject, u32 properties) {
    return OCR_ENOTSUP;
}

ocrSchedulerHeuristicContext_t* hierarchicalSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, u64 contextId) {
    return self->contexts[contextId];
}

/* Setup the context for this contextId */
u8 hierarchicalSchedulerHeuristicRegisterContext(ocrSchedulerHeuristic_t *self, u64 contextId, ocrLocation_t loc) {
    ocrSchedulerHeuristicContext_t *context = self->contexts[contextId];
    ocrSchedulerHeuristicContextHierarchical_t *hcContext = (ocrSchedulerHeuristicContextHierarchical_t*)context;
    ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
    ocrSchedulerObjectFactory_t *rootFact = self->scheduler->pd->schedulerObjectFactories[rootObj->fctId];
    
    hcContext->mySchedulerObject = rootFact->fcts.getSchedulerObjectForLocation(rootFact, rootObj, contextId, OCR_SCHEDULER_OBJECT_MAPPING_PINNED, SCHEDULER_OBJECT_MAPPING_WST | SCHEDULER_OBJECT_CREATE_IF_ABSENT);
    hcContext->mySchedulerObject_2 = rootFact->fcts.getSchedulerObjectForLocation(rootFact, rootObj, contextId*2, OCR_SCHEDULER_OBJECT_MAPPING_PINNED, SCHEDULER_OBJECT_MAPPING_WST | SCHEDULER_OBJECT_CREATE_IF_ABSENT);

    ASSERT(hcContext->mySchedulerObject);
    hcContext->stealSchedulerObjectIndex = (contextId + 1) % self->contextCount;
    return 0;
}

u64 whichEdt;
//Take EDT from workpile
/* Find EDT for the worker to execute - This uses random workstealing to find work if no work is found owned deque */
static u8 hierarchicalSchedulerHeuristicWorkEdtUserInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    ocrSchedulerObject_t edtObj;
    edtObj.guid.guid = NULL_GUID;
    edtObj.guid.metaDataPtr = NULL;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;


//Get current priority group to pick up from
//pick and steal from the same group across workers

    ocrSchedulerHeuristicContextHierarchical_t *hcContext = (ocrSchedulerHeuristicContextHierarchical_t*)context;
    ocrSchedulerObject_t *schedObj;

    u8 retVal;
    u64 appTurn = whichEdt;

    if(appTurn==1){
        schedObj = hcContext->mySchedulerObject;
    }
    else{
        schedObj = hcContext->mySchedulerObject_2;
    }

    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    retVal = fact->fcts.remove(fact, schedObj, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_POP);

    if (edtObj.guid.guid == NULL_GUID) {
        ocrSchedulerObject_t *stealSchedulerObject;
            u32 i;
            for (i = 1; edtObj.guid.guid == NULL_GUID && i < self->contextCount; i++) {
                if(appTurn == 1)
                    hcContext->stealSchedulerObjectIndex = ((context->id + i)*2) % self->contextCount; //simple round robin stealing
                else
                    hcContext->stealSchedulerObjectIndex = (((context->id + i)*2)+1) % self->contextCount; //simple round robin stealing
    
                stealSchedulerObject = ((ocrSchedulerHeuristicContextHierarchical_t*)self->contexts[hcContext->stealSchedulerObjectIndex])->mySchedulerObject;
                if (stealSchedulerObject)
                    retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_STEAL);
            }
    }


    if (edtObj.guid.guid != NULL_GUID)
        taskArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt = edtObj.guid;

    return retVal;
}

u8 hierarchicalSchedulerHeuristicGetWorkInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    switch(taskArgs->kind) {
    case OCR_SCHED_WORK_EDT_USER:
        return hierarchicalSchedulerHeuristicWorkEdtUserInvoke(self, context, opArgs, hints);
    // Unknown ops
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 hierarchicalSchedulerHeuristicGetWorkSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}



//Give EDT to workpile
static u8 hierarchicalSchedulerHeuristicNotifyEdtReadyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrSchedulerHeuristicContextHierarchical_t *hcContext = (ocrSchedulerHeuristicContextHierarchical_t*)context;

    //check if schedobj has children using NULL ptr
    //iterate over some group, based on some metadata associated with schedObj within it

    
    u64 edt_tag;
    { // read EDT hint
//        ASSERT(opArgs->el->kind == OCR_SCHEDULER_OBJECT_EDT);
//        u64 edt_tag = ((ocrGuidImpl_t*)(opArgs->el->guid))->tag;
        ocrFatGuid_t edtGuid = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid;//opArgs->el->guid;
        ocrHint_t edtHints;
        ocrHintInit(&edtHints, OCR_HINT_EDT_T);
        ocrGetHint(edtGuid.guid, &edtHints);
        ocrGetHintValue(&edtHints, OCR_HINT_EDT_PRIORITY, &edt_tag);
        //printf("\nEdt tag = %llu", edt_tag);
    }

    //Let's use first deque for app1 and second deque for app2

    ocrSchedulerObject_t *schedObj;

    if (edt_tag==1) {
        //printf("\nAdding task to deq 2");
        schedObj = hcContext->mySchedulerObject_2;
    }
    else {
        //printf("\nAdding task to deq 1");
        schedObj = hcContext->mySchedulerObject;
    }

    ASSERT(schedObj);
    ocrSchedulerObject_t edtObj;
    edtObj.guid = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    return fact->fcts.insert(fact, schedObj, &edtObj, 0);
}

u8 hierarchicalSchedulerHeuristicNotifyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    switch(notifyArgs->kind) {
    case OCR_SCHED_NOTIFY_EDT_READY:
        return hierarchicalSchedulerHeuristicNotifyEdtReadyInvoke(self, context, opArgs, hints);
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
u8 hierarchicalSchedulerHeuristicGiveSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(opArgs->schedulerObject->kind == OCR_SCHEDULER_OBJECT_EDT);
    ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t*)self->contexts[opArgs->contextId];
    ocrSchedulerHeuristicContextHierarchical_t *hcContext = (ocrSchedulerHeuristicContextHierarchical_t*)context;
    ASSERT(context->actionSet == NULL);

    ocrSchedulerObjectActionSet_t *actionSet = &(hcContext->singleActionSet);
    actionSet->actions = &(hcContext->insertAction);
    ASSERT(actionSet->actions->schedulerObject == hcContext->mySchedulerObject);
    actionSet->actions->args.insert.el = opArgs->schedulerObject;
    context->actionSet = actionSet;
    ASSERT(0);
    return OCR_ENOTSUP;
}
#endif

u8 hierarchicalSchedulerHeuristicNotifySimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 hierarchicalSchedulerHeuristicTransactInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 hierarchicalSchedulerHeuristicTransactSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 hierarchicalSchedulerHeuristicAnalyzeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 hierarchicalSchedulerHeuristicAnalyzeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-HC SCHEDULER_HEURISTIC FACTORY                 */
/******************************************************/

void destructSchedulerHeuristicFactoryHierarchical(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryHierarchical(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactoryHierarchical_t), NONPERSISTENT_CHUNK);
    base->factoryId = factoryId;
    base->instantiate = &newSchedulerHeuristicHierarchical;
    base->destruct = &destructSchedulerHeuristicFactoryHierarchical;
    base->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                 phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), hierarchicalSchedulerHeuristicSwitchRunlevel);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), hierarchicalSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), hierarchicalSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, u64), hierarchicalSchedulerHeuristicGetContext);
    base->fcts.registerContext = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u64, ocrLocation_t), hierarchicalSchedulerHeuristicRegisterContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicGetWorkInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicGetWorkSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicNotifyInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicNotifySimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicTransactInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicTransactSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicAnalyzeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hierarchicalSchedulerHeuristicAnalyzeSimulate);

    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_HIERARCHICAL */
