/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 * - A scheduler heuristic to distribute tasks and data across space and time
 *   on a distributed system. The placement decision is done by select nodes
 *   known as scheduling nodes. At this time, the heuristic supports only one
 *   scheduling node, configured to be node 0. So, all other nodes in the
 *   system communicate to this central scheduling node to manage the scheduling
 *   of their tasks and data. This mechanism is obviously not ideal but will
 *   serve as a baseline for future improvements.
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_ST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "scheduler-heuristic/st/st-scheduler-heuristic.h"

/******************************************************/
/* OCR-ST SCHEDULER_HEURISTIC                         */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicSt(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicSt_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    ocrSchedulerHeuristicSt_t *derived = (ocrSchedulerHeuristicSt_t*)self;
    derived->schedulerLocation = 0;
    return self;
}

static void initializeContextSt(ocrSchedulerHeuristicContext_t *context, u64 contextId) {
    context->id = contextId;
    context->actionSet = NULL;
    context->cost = NULL;
    context->properties = 0;

    ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)context;
    stContext->stealSchedulerObjectIndex = ((u64)-1);
    stContext->mySchedulerObject = NULL;
    return;
}

u8 stSchedulerHeuristicSwitchRunlevel(ocrSchedulerHeuristic_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        ASSERT(self->scheduler);
        self->contextCount = PD->workerCount; //Shared mem heuristic
        ASSERT(self->contextCount > 0);
        break;
    }
    case RL_MEMORY_OK:
    {
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
            u32 i;
            self->contexts = (ocrSchedulerHeuristicContext_t **)PD->fcts.pdMalloc(PD, self->contextCount * sizeof(ocrSchedulerHeuristicContext_t*));
            ocrSchedulerHeuristicContextSt_t *contextAlloc = (ocrSchedulerHeuristicContextSt_t *)PD->fcts.pdMalloc(PD, self->contextCount * sizeof(ocrSchedulerHeuristicContextSt_t));
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)&(contextAlloc[i]);
                initializeContextSt(context, i);
                self->contexts[i] = context;
                context->id = i;
                context->location = PD->myLocation;
                context->actionSet = NULL;
                context->cost = NULL;
                context->properties = 0;
                ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)context;
                stContext->stealSchedulerObjectIndex = ((u64)-1);
                stContext->mySchedulerObject = NULL;
                stContext->mapIterator = NULL;
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
#if ST
            u32 i;
            ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
            ocrSchedulerObjectFactory_t *rootFact = PD->schedulerObjectFactories[rootObj->fctId];
            ocrSchedulerObjectFactory_t *mapFact = PD->schedulerObjectFactories[schedulerObjectMap_id];
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)self->contexts[i];
                stContext->mySchedulerObject = rootFact->fcts.getSchedulerObjectForLocation(rootFact, rootObj, i, OCR_SCHEDULER_OBJECT_MAPPING_WORKER, 0);
                ASSERT(stContext->mySchedulerObject);
                stContext->stealSchedulerObjectIndex = (i + 1) % self->contextCount;
                stContext->mapIterator = mapFact->fcts.createIterator(mapFact, rootObj->dbMap, 0);
            }
#endif
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

void stSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 stSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, u32 properties) {
    return OCR_ENOTSUP;
}

ocrSchedulerHeuristicContext_t* stSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, ocrLocation_t loc) {
    ASSERT(loc == self->scheduler->pd->myLocation);
    ocrWorker_t * worker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    return self->contexts[worker->id];
}

/* Find EDT for the worker to execute - This uses random workstealing to find work if no work is found owned deque */
static u8 stSchedulerHeuristicWorkEdtUserInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    ocrSchedulerObject_t edtObj;
    edtObj.guid.guid = NULL_GUID;
    edtObj.guid.metaDataPtr = NULL;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;

    //First try to pop from own deque
    ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)context;
    ocrSchedulerObject_t *schedObj = stContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    u8 retVal = fact->fcts.remove(fact, schedObj, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_TAIL);

    //If pop fails, then try to steal from other deques
    if (edtObj.guid.guid == NULL_GUID) {
        //First try to steal from the last deque that was visited (probably had a successful steal)
        ocrSchedulerObject_t *stealSchedulerObject = ((ocrSchedulerHeuristicContextSt_t*)self->contexts[stContext->stealSchedulerObjectIndex])->mySchedulerObject;
        ASSERT(stealSchedulerObject);
        retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_HEAD); //try cached deque first

        //If cached steal failed, then restart steal loop from starting index
        ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
        ocrSchedulerObjectFactory_t *sFact = self->scheduler->pd->schedulerObjectFactories[rootObj->fctId];
        while (edtObj.guid.guid == NULL_GUID && sFact->fcts.count(sFact, rootObj, (SCHEDULER_OBJECT_COUNT_EDT | SCHEDULER_OBJECT_COUNT_RECURSIVE) ) != 0) {
            u32 i;
            for (i = 1; edtObj.guid.guid == NULL_GUID && i < self->contextCount; i++) {
                stContext->stealSchedulerObjectIndex = (context->id + i) % self->contextCount; //simple round robin stealing
                stealSchedulerObject = ((ocrSchedulerHeuristicContextSt_t*)self->contexts[stContext->stealSchedulerObjectIndex])->mySchedulerObject;
                if (stealSchedulerObject)
                    retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_HEAD);
            }
        }
    }

    if (edtObj.guid.guid != NULL_GUID)
        taskArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt = edtObj.guid;

    return retVal;
}

u8 stSchedulerHeuristicGetWorkInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContext_t *context = self->fcts.getContext(self, opArgs->location);
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    switch(taskArgs->kind) {
    case OCR_SCHED_WORK_EDT_USER:
        return stSchedulerHeuristicWorkEdtUserInvoke(self, context, opArgs, hints);
    // Unknown ops
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 stSchedulerHeuristicGetWorkSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

static u8 createDbspace(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrGuid_t dbGuid, u64 dbSize, void *ptr) {
#if ST
    //Create a Dbspace scheduler object
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    paramListSchedulerObjectDbspace_t dbspaceParams;
    dbspaceParams.base.config = 0;
    dbspaceParams.base.guidRequired = 0;
    dbspaceParams.dbGuid = dbGuid;
    dbspaceParams.dbSize = dbSize;
    dbspaceParams.dataPtr = ptr;
    ocrSchedulerObjectFactory_t *dbspaceFact = pd->schedulerObjectFactories[schedulerObjectDbspace_id];
    ocrSchedulerObject_t *dbspaceObj = dbspaceFact->fcts.create(dbspaceFact, (ocrParamList_t*)&dbspaceParams);
    ASSERT(dbspaceObj);

    //Insert the Dbspace object into root dbMap
    DPRINTF(DEBUG_LVL_VERB, "DB node. Key: %lu Value: %lu\n", dbGuid, dbspaceObj);
    stContext->mapIterator->data = dbGuid; //Setup key on iterator
    ocrSchedulerObjectPdspace_t *pdspaceObj = (ocrSchedulerObjectPdspace_t*)self->scheduler->rootObj;
    ocrSchedulerObjectFactory_t *mapFact = pd->schedulerObjectFactories[pdspaceObj->dbMap->fctId];
    mapFact->fcts.iterate(mapFact, stContext->mapIterator, SCHEDULER_OBJECT_ITERATE_SEARCH_KEY); //Check for existing object from same guid
    ASSERT(stContext->mapIterator->data == NULL); //We assert that guid is not reused but it is possible. In case of reuse, remove mapped object first.
    mapFact->fcts.insert(mapFact, dbspaceObj, stContext->mapIterator, (SCHEDULER_OBJECT_INSERT_INPLACE | SCHEDULER_OBJECT_INSERT_POSITION_ITERATOR));

    //Transact this scheduler object to the scheduler node
    ocrSchedulerHeuristicSt_t *derived = (ocrSchedulerHeuristicSt_t*)self;
    if (pd->myLocation != derived->schedulerLocation) {
        PD_MSG_STACK(msg);
        getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_SCHED_TRANSACT
        msg.type = PD_MSG_SCHED_TRANSACT | PD_MSG_REQUEST;
        PD_MSG_FIELD_IO(schedArgs).base.heuristicId = self->factoryId;
        PD_MSG_FIELD_IO(schedArgs).properties = OCR_TRANSACT_PROP_TRANSFER;
        PD_MSG_FIELD_IO(schedArgs).schedObj = dbspaceObj;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));
#undef PD_MSG
#undef PD_TYPE
    }
#endif
    return 0;
}

static u8 stSchedulerHeuristicNotifyDbCreateInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrDataBlock_t *db = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_DB_CREATE).guid.metaDataPtr;
    ASSERT(db);
    return createDbspace(self, context, db->guid, db->size, db->ptr);
}

static u8 stSchedulerHeuristicNotifyEdtSatisfiedInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrTask_t *task __attribute__((unused)) = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_SATISFIED).guid.metaDataPtr;
    ASSERT(task);
    //...//Send task deps and associated modes to scheduler node for space/time analysis
    return 0;
}

static u8 stSchedulerHeuristicNotifyEdtReadyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)context;
    ocrSchedulerObject_t *schedObj = stContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObject_t edtObj;
    edtObj.guid = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    return fact->fcts.insert(fact, schedObj, &edtObj, NULL, (SCHEDULER_OBJECT_INSERT_AFTER | SCHEDULER_OBJECT_INSERT_POSITION_TAIL));
}

u8 stSchedulerHeuristicNotifyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristicContext_t *context = self->fcts.getContext(self, opArgs->location);
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    switch(notifyArgs->kind) {
    case OCR_SCHED_NOTIFY_DB_CREATE:
        return stSchedulerHeuristicNotifyDbCreateInvoke(self, context, opArgs, hints);
    case OCR_SCHED_NOTIFY_EDT_SATISFIED:
        return stSchedulerHeuristicNotifyEdtSatisfiedInvoke(self, context, opArgs, hints);
    case OCR_SCHED_NOTIFY_EDT_READY:
        return stSchedulerHeuristicNotifyEdtReadyInvoke(self, context, opArgs, hints);
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
    // Unknown ops
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

#if 0 //Example simulate op
u8 stSchedulerHeuristicGiveSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(opArgs->schedulerObject->kind == OCR_SCHEDULER_OBJECT_EDT);
    ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t*)self->contexts[opArgs->contextId];
    ocrSchedulerHeuristicContextSt_t *stContext = (ocrSchedulerHeuristicContextSt_t*)context;
    ASSERT(context->actionSet == NULL);

    ocrSchedulerObjectActionSet_t *actionSet = &(stContext->singleActionSet);
    actionSet->actions = &(stContext->insertAction);
    ASSERT(actionSet->actions->schedulerObject == stContext->mySchedulerObject);
    actionSet->actions->args.insert.el = opArgs->schedulerObject;
    context->actionSet = actionSet;
    ASSERT(0);
    return OCR_ENOTSUP;
}
#endif

u8 stSchedulerHeuristicNotifySimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 stSchedulerHeuristicTransactInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 stSchedulerHeuristicTransactSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 stSchedulerHeuristicAnalyzeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 stSchedulerHeuristicAnalyzeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-ST SCHEDULER_HEURISTIC FACTORY                 */
/******************************************************/

void destructSchedulerHeuristicFactorySt(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactorySt(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactorySt_t), NONPERSISTENT_CHUNK);
    base->factoryId = factoryId;
    base->instantiate = &newSchedulerHeuristicSt;
    base->destruct = &destructSchedulerHeuristicFactorySt;
    base->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                 phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), stSchedulerHeuristicSwitchRunlevel);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), stSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), stSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, ocrLocation_t), stSchedulerHeuristicGetContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicGetWorkInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicGetWorkSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicNotifyInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicNotifySimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicTransactInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicTransactSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicAnalyzeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), stSchedulerHeuristicAnalyzeSimulate);

    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_ST */
