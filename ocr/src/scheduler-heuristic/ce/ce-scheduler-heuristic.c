/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 * - A scheduler heuristic for CE's on a TG machine
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "extensions/ocr-hints.h"
#include "scheduler-heuristic/ce/ce-scheduler-heuristic.h"
#include "scheduler-object/wst/wst-scheduler-object.h"
#include "policy-domain/ce/ce-policy.h"

//Temporary until we get introspection support
#include "task/hc/hc-task.h"

#define DEBUG_TYPE SCHEDULER_HEURISTIC

/******************************************************/
/* OCR-CE SCHEDULER_HEURISTIC                         */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicCe(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicCe_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
    derived->workCount = 0;
    derived->inPendingCount = 0;
    derived->pendingXeCount = 0;
    derived->workRequestStartIndex = 0;
    derived->outWorkVictimsAvailable = 0;
    return self;
}

void initializeContext(ocrSchedulerHeuristicContext_t *context, u64 contextId) {
    context->id = contextId;
    context->location = INVALID_LOCATION;
    context->actionSet = NULL;
    context->cost = NULL;
    context->properties = 0;

    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ceContext->stealSchedulerObjectIndex = ((u64)-1);
    ceContext->mySchedulerObject = NULL;
    ceContext->inWorkRequestPending = false;
    ceContext->outWorkRequestPending = false;
    ceContext->canAcceptWorkRequest = false;
    return;
}

u8 ceSchedulerHeuristicSwitchRunlevel(ocrSchedulerHeuristic_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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

            DPRINTF(DEBUG_LVL_VVERB, "ContextCount: %lu\n", scheduler->contextCount);
            self->contextCount = scheduler->contextCount;
            self->contexts = (ocrSchedulerHeuristicContext_t **)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContext_t*));
            ocrSchedulerHeuristicContextCe_t *contextAlloc = (ocrSchedulerHeuristicContextCe_t *)pd->fcts.pdMalloc(pd, self->contextCount * sizeof(ocrSchedulerHeuristicContextCe_t));
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *context = (ocrSchedulerHeuristicContext_t *)&(contextAlloc[i]);
                initializeContext(context, i);
                self->contexts[i] = context;
            }

            ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
            derived->workRequestStartIndex = ((ocrPolicyDomainCe_t*)pd)->xeCount;
            derived->outWorkVictimsAvailable = pd->neighborCount;
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

void ceSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

ocrSchedulerHeuristicContext_t* ceSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, u64 contextId) {
    return self->contexts[contextId];
}

/* Setup the context for this contextId */
u8 ceSchedulerHeuristicRegisterContext(ocrSchedulerHeuristic_t *self, u64 contextId, ocrLocation_t loc) {
    ocrSchedulerHeuristicContext_t *context = self->contexts[contextId];
    ASSERT(context->location == INVALID_LOCATION && loc != INVALID_LOCATION);
    context->location = loc;

    //Reserve a deque for this context
    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)self->scheduler->rootObj;
    ASSERT(contextId < wstSchedObj->numDeques);
    ceContext->mySchedulerObject = wstSchedObj->deques[contextId];
    ASSERT(ceContext->mySchedulerObject);
    ceContext->stealSchedulerObjectIndex = (contextId + 1) % wstSchedObj->numDeques;

    //Set deque's mapping to this context
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[ceContext->mySchedulerObject->fctId];
    fact->fcts.setLocationForSchedulerObject(fact, ceContext->mySchedulerObject, contextId, OCR_SCHEDULER_OBJECT_MAPPING_PINNED);

    if (AGENT_FROM_ID(loc) == ID_AGENT_CE) {
        ceContext->canAcceptWorkRequest = true;
        DPRINTF(DEBUG_LVL_VVERB, "[CE %lx] Registering CE location: %lx\n", self->scheduler->pd->myLocation, loc);
    } else {
        DPRINTF(DEBUG_LVL_VVERB, "[CE %lx] Registering XE location: %lx\n", self->scheduler->pd->myLocation, loc);
    }
    return 0;
}

/* Find EDT for the worker to execute - This uses random workstealing to find work if no work is found owned deque */
static u8 ceWorkStealingGet(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrFatGuid_t *fguid) {
    ASSERT(fguid->guid == NULL_GUID);
    ASSERT(fguid->metaDataPtr == NULL);

    ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
    ocrSchedulerObject_t *rootObj = self->scheduler->rootObj;
    if (derived->workCount == 0) {
#if 0 //Enable to make scheduler chatty
        return 0;
#else
        return 1;
#endif
    }

    ocrSchedulerObject_t edtObj;
    edtObj.guid.guid = NULL_GUID;
    edtObj.guid.metaDataPtr = NULL;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;

    //First try to pop from own deque
    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ocrSchedulerObject_t *schedObj = ceContext->mySchedulerObject;
    ASSERT(schedObj);
    ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
    u8 retVal = fact->fcts.remove(fact, schedObj, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_POP);

#if 1 //Turn off to disable stealing (serialize execution)
    //If pop fails, then try to steal from other deques
    if (edtObj.guid.guid == NULL_GUID) {
        //First try to steal from the last deque that was visited (probably had a successful steal)
        ocrSchedulerObjectWst_t *wstSchedObj = (ocrSchedulerObjectWst_t*)rootObj;
        ocrSchedulerObject_t *stealSchedulerObject = wstSchedObj->deques[ceContext->stealSchedulerObjectIndex];
        ASSERT(stealSchedulerObject->fctId == schedObj->fctId);
        retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_POP); //try cached deque first

        //If cached steal failed, then restart steal loop from starting index
        u32 i;
        for (i = 1; edtObj.guid.guid == NULL_GUID && i < wstSchedObj->numDeques; i++) {
            ceContext->stealSchedulerObjectIndex = (context->id + i) % wstSchedObj->numDeques;
            stealSchedulerObject = wstSchedObj->deques[ceContext->stealSchedulerObjectIndex];
            ASSERT(stealSchedulerObject->fctId == schedObj->fctId);
            retVal = fact->fcts.remove(fact, stealSchedulerObject, OCR_SCHEDULER_OBJECT_EDT, 1, &edtObj, NULL, SCHEDULER_OBJECT_REMOVE_DEQ_POP);
        }
    }
#endif

    if (edtObj.guid.guid != NULL_GUID) {
        ASSERT(retVal == 0);
        *fguid = edtObj.guid;
        derived->workCount--;
    } else {
        ASSERT(retVal != 0);
        ASSERT(0); //Check done early
    }
    return retVal;
}

static u8 ceSchedulerHeuristicWorkEdtUserInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    DPRINTF(DEBUG_LVL_VVERB, "TAKE_WORK_INVOKE from %lx \n", context->location);
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    ocrFatGuid_t *fguid = &(taskArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt);
    u8 retVal = ceWorkStealingGet(self, context, fguid);
    if (retVal != 0) {
        ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
        ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
        ASSERT(ceContext->inWorkRequestPending == false);
        DPRINTF(DEBUG_LVL_VVERB, "TAKE_WORK_INVOKE from %lx (pending)\n", context->location);
        // If the receiver is an XE, put it to sleep
        u64 agentId = AGENT_FROM_ID(context->location);
        if ((agentId >= ID_AGENT_XE0) && (agentId <= ID_AGENT_XE7)) {
            DPRINTF(DEBUG_LVL_INFO, "XE %lx put to sleep\n", context->location);
            hal_sleep(agentId);
            derived->pendingXeCount++;
        }
        ceContext->inWorkRequestPending = true;
        derived->inPendingCount++;
        retVal = OCR_SCHEDULER_GET_WORK_PROP_REQUEST_PENDING;
    } else {
        retVal = OCR_SCHEDULER_GET_WORK_PROP_RESPONSE_WORK_SEND;
        DPRINTF(DEBUG_LVL_VVERB, "TAKE_WORK_INVOKE from %lx (found)\n", context->location);
    }
    return retVal;
}

u8 ceSchedulerHeuristicGetWorkInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpWorkArgs_t *taskArgs = (ocrSchedulerOpWorkArgs_t*)opArgs;
    switch(taskArgs->kind) {
    case OCR_SCHED_WORK_EDT_USER:
        return ceSchedulerHeuristicWorkEdtUserInvoke(self, context, opArgs, hints);
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 ceSchedulerHeuristicGetWorkSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

static u8 handleEmptyResponse(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context) {
    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
    ASSERT(AGENT_FROM_ID(context->location) == ID_AGENT_CE);
    ASSERT(ceContext->outWorkRequestPending);
    ceContext->outWorkRequestPending = false;
    derived->outWorkVictimsAvailable++;
    return 0;
}

static u8 ceSchedulerHeuristicNotifyEdtReadyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    DPRINTF(DEBUG_LVL_VVERB, "GIVE_WORK_INVOKE from %lx\n", context->location);
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    ocrFatGuid_t fguid = notifyArgs->OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_READY).guid;

    if (fguid.guid == NULL_GUID) {
        return handleEmptyResponse(self, context);
    }

    ocrTask_t *task = (ocrTask_t*)fguid.metaDataPtr;
    ASSERT(task);

    //Schedule EDT according to its DB affinity
    ocrSchedulerHeuristicContext_t *insertContext = context;
    u64 affinitySlot = ((u64)-1);
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ASSERT(ocrGetHint(task->guid, &edtHint) == 0);
    if (ocrGetHintValue(&edtHint, OCR_HINT_EDT_SLOT_MAX_ACCESS, &affinitySlot) == 0) {
        ASSERT(affinitySlot < task->depc);
        ocrTaskHc_t *hcTask = (ocrTaskHc_t*)task; //TODO:This is temporary until we get proper introspection support
        ocrEdtDep_t *depv = hcTask->resolvedDeps;
        ocrGuid_t dbGuid = depv[affinitySlot].guid;
        ocrDataBlock_t *db = NULL;
        pd->guidProviders[0]->fcts.getVal(pd->guidProviders[0], dbGuid, (u64*)(&(db)), NULL);
        ASSERT(db);
        u64 dbMemAffinity = ((u64)-1);
        ocrHint_t dbHint;
        ocrHintInit(&dbHint, OCR_HINT_DB_T);
        ASSERT(ocrGetHint(dbGuid, &dbHint) == 0);
        if (ocrGetHintValue(&dbHint, OCR_HINT_DB_MEM_AFFINITY, &dbMemAffinity) == 0) {
            ocrLocation_t myLoc = pd->myLocation;
            ocrLocation_t dbLoc = dbMemAffinity;
            ocrLocation_t affinityLoc = dbLoc;
            u64 dbLocUnt = UNIT_FROM_ID(dbLoc);
            u64 dbLocBlk = BLOCK_FROM_ID(dbLoc);
            if (dbLocUnt != UNIT_FROM_ID(myLoc)) {
                affinityLoc = MAKE_CORE_ID(0, 0, 0, dbLocUnt, 0, ID_AGENT_CE); //Map it to block 0 for dbLocUnt
            } else if (dbLocBlk != BLOCK_FROM_ID(myLoc)) {
                affinityLoc = MAKE_CORE_ID(0, 0, 0, dbLocUnt, dbLocBlk, ID_AGENT_CE); //Map it to dbLocBlk of current unit
            }
            u32 i;
            bool found = false;
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *ctxt = self->contexts[i];
                if (ctxt->location == affinityLoc) {
                    insertContext = ctxt;
                    found = true;
                    break;
                }
            }
            ASSERT(found);
        }
    }

    ocrSchedulerHeuristicContextCe_t *ceInsertContext = (ocrSchedulerHeuristicContextCe_t*)insertContext;
    ocrSchedulerObject_t *insertSchedObj = ceInsertContext->mySchedulerObject;
    ASSERT(insertSchedObj);
    ocrSchedulerObject_t edtObj;
    edtObj.guid = fguid;
    edtObj.kind = OCR_SCHEDULER_OBJECT_EDT;
    ocrSchedulerObjectFactory_t *fact = pd->schedulerObjectFactories[insertSchedObj->fctId];
    ASSERT(fact->fcts.insert(fact, insertSchedObj, &edtObj, 0) == 0);
    ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
    derived->workCount++;
    if (AGENT_FROM_ID(context->location) == ID_AGENT_CE) {
        ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
        ASSERT(ceContext->outWorkRequestPending);
        ceContext->outWorkRequestPending = false;
        derived->outWorkVictimsAvailable++;
    }
    return 0;
}

u8 ceSchedulerHeuristicNotifyInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerOpNotifyArgs_t *notifyArgs = (ocrSchedulerOpNotifyArgs_t*)opArgs;
    switch(notifyArgs->kind) {
    case OCR_SCHED_NOTIFY_EDT_READY:
        return ceSchedulerHeuristicNotifyEdtReadyInvoke(self, context, opArgs, hints);
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
    case OCR_SCHED_NOTIFY_DB_CREATE:
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 ceSchedulerHeuristicNotifySimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 ceSchedulerHeuristicTransactInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 ceSchedulerHeuristicTransactSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 ceSchedulerHeuristicAnalyzeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 ceSchedulerHeuristicAnalyzeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

static u8 makeWorkRequest(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context) {
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    DPRINTF(DEBUG_LVL_VVERB, "MAKE_WORK_REQUEST to %lx\n", context->location);
    ASSERT(AGENT_FROM_ID(context->location) == ID_AGENT_CE);
    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ASSERT(ceContext->outWorkRequestPending == false);
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.srcLocation = pd->myLocation;
    msg.destLocation = context->location;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
    msg.type = PD_MSG_SCHED_GET_WORK | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(schedArgs).kind = OCR_SCHED_WORK_EDT_USER;
    PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.guid = NULL_GUID;
    PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.metaDataPtr = NULL;
    PD_MSG_FIELD_I(properties) = OCR_SCHEDULER_GET_WORK_PROP_REQUEST;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    if (returnCode == 0) {
        ceContext->outWorkRequestPending = true;
        ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
        derived->outWorkVictimsAvailable--;
    }
    return returnCode;
}

static bool respondWorkRequest(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrFatGuid_t *fguid, u32 properties) {
    DPRINTF(DEBUG_LVL_VVERB, "RESPOND_WORK_REQUEST to %lx\n", context->location);
    ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
    ASSERT(ceContext->inWorkRequestPending);
    if (fguid) {
        ASSERT(fguid->guid != NULL_GUID);
        ASSERT(properties == OCR_SCHEDULER_GET_WORK_PROP_RESPONSE_WORK_SEND);
    }

    //TODO: For now pretend we are an XE until
    //we get the transact messages working
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.srcLocation = context->location;
    msg.destLocation = pd->myLocation;

    // If the receiver is an XE, wake it up
    u64 agentId = AGENT_FROM_ID(context->location);
    if ((agentId >= ID_AGENT_XE0) && (agentId <= ID_AGENT_XE7)) {
        hal_wake(agentId);
        DPRINTF(DEBUG_LVL_INFO, "XE %lx woken up\n", context->location);
        derived->pendingXeCount--;
    }

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
    msg.type = PD_MSG_SCHED_GET_WORK | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(schedArgs).base.seqId = 0;
    PD_MSG_FIELD_IO(schedArgs).kind = OCR_SCHED_WORK_EDT_USER;
    if (fguid) {
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt = *fguid;
    } else {
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.guid = NULL_GUID;
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.metaDataPtr = NULL;
    }
    PD_MSG_FIELD_I(properties) = properties;
    ASSERT(pd->fcts.processMessage(pd, &msg, false) == 0);
#undef PD_MSG
#undef PD_TYPE
    ceContext->inWorkRequestPending = false;
    derived->inPendingCount--;
    return true;
}

u8 ceSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, u32 properties) {
    ocrPolicyDomain_t *pd = self->scheduler->pd;
    u32 i;
    switch(properties) {
    case OCR_SCHEDULER_HEURISTIC_UPDATE_PROP_IDLE: {
            ocrSchedulerHeuristicCe_t *derived = (ocrSchedulerHeuristicCe_t*)self;
            DPRINTF(DEBUG_LVL_VVERB, "IDLE [work: %lu pending: %lu victims: %lu]\n", derived->workCount, derived->inPendingCount, derived->outWorkVictimsAvailable);
            if (derived->inPendingCount == 0)
                break; //Nobody is waiting. Great!

            ASSERT(derived->inPendingCount <= self->contextCount);

            if (derived->workCount != 0) {
                //We have work... some are waiting... start giving
                ocrFatGuid_t fguid;
                for (i = 0; i < self->contextCount; i++) {
                    ocrSchedulerHeuristicContext_t *context = self->contexts[i];
                    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
                    if (ceContext->inWorkRequestPending &&
                        ((derived->workCount > derived->pendingXeCount) ||
                         (AGENT_FROM_ID(context->location) != ID_AGENT_CE)))
                    {
                        fguid.guid = NULL_GUID;
                        fguid.metaDataPtr = NULL;
                        if (ceWorkStealingGet(self, context, &fguid) == 0) {
                            respondWorkRequest(self, context, &fguid, OCR_SCHEDULER_GET_WORK_PROP_RESPONSE_WORK_SEND);
                        } else {
                            //No work left
                            ASSERT(derived->workCount == 0);
                            break;
                        }
                    }
                }
            }

            if (derived->workCount == 0 && derived->pendingXeCount != 0 && derived->outWorkVictimsAvailable != 0) {
                //No work left... some are still waiting... so, try to find work
                ASSERT(derived->outWorkVictimsAvailable <= pd->neighborCount);
                u32 startIndex = derived->workRequestStartIndex;
                ASSERT(startIndex < self->contextCount);
                for (i = 0; i < self->contextCount && derived->outWorkVictimsAvailable != 0; i++) {
                    u32 contextId = (i + startIndex) % self->contextCount;
                    ocrSchedulerHeuristicContext_t *context = self->contexts[i];
                    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
                    if (ceContext->canAcceptWorkRequest && ceContext->outWorkRequestPending == false) {
                        u8 returnCode = makeWorkRequest(self, context);
                        if (returnCode != 0) {
                            DPRINTF(DEBUG_LVL_VVERB, "MAKE WORK REQUEST ERROR CODE: %u\n", returnCode);
                            if (returnCode == 2) { //Location is dead
                                ASSERT(context->location != pd->parentLocation); //Make sure parent is not dead
                                ceContext->canAcceptWorkRequest = false;
                                derived->outWorkVictimsAvailable--;
                            }
                        } else {
                            derived->workRequestStartIndex = (contextId + 1) % self->contextCount; //Restart next iteration *after* this context
                            ASSERT(ceContext->outWorkRequestPending);
                            ASSERT(derived->outWorkVictimsAvailable <= pd->neighborCount);
                        }
                    }
                }
            }

            //If block0 CE of unit != unit0, (i.e. myLocation != parentLocation), then ensure work request is sent to parent
            if (((pd->myLocation & ID_BLOCK_MASK) == 0) && pd->myLocation != pd->parentLocation && derived->workCount == 0) {
                //Totally dry, time to make sure parent receives work request
                for (i = 0; i < self->contextCount; i++) {
                    ocrSchedulerHeuristicContext_t *context = self->contexts[i];
                    ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
                    if (context->location == pd->parentLocation) {
                        ASSERT(AGENT_FROM_ID(context->location) == ID_AGENT_CE);
                        if (ceContext->canAcceptWorkRequest && ceContext->outWorkRequestPending == false) {
                            DPRINTF(DEBUG_LVL_VVERB, "FORCE MAKE WORK REQUEST to %lx\n", context->location);
                            u8 returnCode = 0;
                            do {
                                returnCode = makeWorkRequest(self, context);
                                if (returnCode != 0) DPRINTF(DEBUG_LVL_VVERB, "MAKE WORK ERROR CODE: %u\n", returnCode);
                                if (returnCode == 2) { //Location is dead
                                    ASSERT(0); //We should not hit this
                                    ASSERT(context->location != pd->parentLocation); //Make sure parent is not dead
                                    ceContext->canAcceptWorkRequest = false;
                                    derived->outWorkVictimsAvailable--;
                                    break;
                                }
                            } while(returnCode != 0);
                            if (returnCode == 0) DPRINTF(DEBUG_LVL_VVERB, "MAKE WORK REQUEST SUCCESS!\n");
                        }
                    }
                }
            }
        }
        break;
    case OCR_SCHEDULER_HEURISTIC_UPDATE_PROP_SHUTDOWN: {
            for (i = 0; i < self->contextCount; i++) {
                ocrSchedulerHeuristicContext_t *context = self->contexts[i];
                ocrSchedulerHeuristicContextCe_t *ceContext = (ocrSchedulerHeuristicContextCe_t*)context;
                //Send shutdown response to all XEs in my block.
                //Also, if I am the block0 CE of my unit, then send shutdown to other blocks CEs only in my unit
                if ((ceContext->inWorkRequestPending) &&                                                      /* context has request pending, and ... */
                    ((AGENT_FROM_ID(context->location) != ID_AGENT_CE) ||                                     /* either, context is a XE, or ...*/
                     (((pd->myLocation & ID_BLOCK_MASK) == 0) && (context->location != pd->parentLocation)))) /* I am the block0 CE of my unit and will respond to other CEs in my unit only */
                {
                    DPRINTF(DEBUG_LVL_VVERB, "SCHEDULER SHUTDOWN RESPONSE to %lx\n", context->location);
                    respondWorkRequest(self, context, NULL, OCR_SCHEDULER_GET_WORK_PROP_RESPONSE_SHUTDOWN);
                }
            }
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

/******************************************************/
/* OCR-CE SCHEDULER_HEURISTIC FACTORY                 */
/******************************************************/

void destructSchedulerHeuristicFactoryCe(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryCe(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactoryCe_t), NONPERSISTENT_CHUNK);
    base->factoryId = factoryId;
    base->instantiate = &newSchedulerHeuristicCe;
    base->destruct = &destructSchedulerHeuristicFactoryCe;
    base->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                 phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), ceSchedulerHeuristicSwitchRunlevel);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), ceSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), ceSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, u64), ceSchedulerHeuristicGetContext);
    base->fcts.registerContext = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u64, ocrLocation_t), ceSchedulerHeuristicRegisterContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicGetWorkInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicGetWorkSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicNotifyInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicNotifySimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicTransactInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicTransactSimulate);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicAnalyzeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), ceSchedulerHeuristicAnalyzeSimulate);

    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_CE */
