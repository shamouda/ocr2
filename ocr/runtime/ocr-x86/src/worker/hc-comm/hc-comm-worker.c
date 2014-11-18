/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_HC_COMM

#include "debug.h"
#include "ocr-db.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-worker.h"
#include "worker/hc/hc-worker.h"
#include "worker/hc-comm/hc-comm-worker.h"
#include "ocr-errors.h"

#include "experimental/ocr-placer.h"
#include "extensions/ocr-affinity.h"

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-HC COMMUNICATION WORKER                        */
/* Extends regular HC workers                         */
/******************************************************/

ocrGuid_t processRequestEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrPolicyMsg_t * msg = (ocrPolicyMsg_t *) paramv[0];
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    // This is meant to execute incoming request and asynchronously processed responses (two-way asynchronous)
    // Regular responses are routed back to requesters by the scheduler and are processed by them.
    ASSERT((msg->type & PD_MSG_REQUEST) ||
        ((msg->type & PD_MSG_RESPONSE) && ((msg->type & PD_MSG_TYPE_ONLY) == PD_MSG_DB_ACQUIRE)));
    // Important to read this before calling processMessage. If the message requires
    // a response, the runtime reuses the request's message to post the response.
    // Hence there's a race between this code and the code posting the response.
    bool processResponse = !!(msg->type & PD_MSG_RESPONSE); // mainly for debug
    // DB_ACQUIRE are potentially asynchronous
    bool syncProcess = ((msg->type & PD_MSG_TYPE_ONLY) != PD_MSG_DB_ACQUIRE);
    bool toBeFreed = !(msg->type & PD_MSG_REQ_RESPONSE);
    DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Process incoming EDT request @ %p of type 0x%x\n", msg, msg->type);
    u8 res = pd->fcts.processMessage(pd, msg, syncProcess);
    DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: [done] Process incoming EDT @ %p request of type 0x%x\n", msg, msg->type);
    // Either flagged or was an asynchronous processing, the implementation should
    // have setup a callback and we can free the request message
    if (toBeFreed || (!syncProcess && (res == OCR_EPEND))) {
        // Makes sure the runtime doesn't try to reuse this message
        // even though it was not supposed to issue a response.
        // If that's the case, this check is racy
        ASSERT(!(msg->type & PD_MSG_RESPONSE) || processResponse);
        DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Deleted incoming EDT request @ %p of type 0x%x\n", msg, msg->type);
        // if request was an incoming one-way we can delete the message now.
        pd->fcts.pdFree(pd, msg);
    }

    return NULL_GUID;
}

static u8 takeFromSchedulerAndSend(ocrPolicyDomain_t * pd) {
    // When the communication-worker is not stopping only a single iteration is
    // executed. Otherwise it is executed until the scheduler's 'take' do not
    // return any more work.
    ocrMsgHandle_t * outgoingHandle = NULL;
    ocrPolicyMsg_t msgCommTake;
    u8 ret = 0;
    getCurrentEnv(NULL, NULL, NULL, &msgCommTake);
    ocrFatGuid_t handlerGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
    //IMPL: MSG_COMM_TAKE implementation must be consistent across PD, Scheduler and Worker.
    // We expect the PD to fill-in the guids pointer with an ocrMsgHandle_t pointer
    // to be processed by the communication worker or NULL.
    //PERF: could request 'n' for internal comm load balancing (outgoing vs pending vs incoming).
    #define PD_MSG (&msgCommTake)
    #define PD_TYPE PD_MSG_COMM_TAKE
    msgCommTake.type = PD_MSG_COMM_TAKE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guids) = &handlerGuid;
    PD_MSG_FIELD(extra) = 0; /*unused*/
    PD_MSG_FIELD(guidCount) = 1;
    PD_MSG_FIELD(properties) = 0;
    PD_MSG_FIELD(type) = OCR_GUID_COMM;
    ret = pd->fcts.processMessage(pd, &msgCommTake, true);
    if (!ret && (PD_MSG_FIELD(guidCount) != 0)) {
        ASSERT(PD_MSG_FIELD(guidCount) == 1); //LIMITATION: single guid returned by comm take
        ocrFatGuid_t handlerGuid = PD_MSG_FIELD(guids[0]);
        ASSERT(handlerGuid.metaDataPtr != NULL);
        outgoingHandle = (ocrMsgHandle_t *) handlerGuid.metaDataPtr;
    #undef PD_MSG
    #undef PD_TYPE
        if (outgoingHandle != NULL) {
            // This code handles the pd's outgoing messages. They can be requests or responses.
            DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: outgoing handle comm take successful handle=%p, msg=%p type=0x%x\n",
                    outgoingHandle, outgoingHandle->msg, outgoingHandle->msg->type);
            //We can never have an outgoing handle with the response ptr set because
            //when we process an incoming request, we lose the handle by calling the
            //pd's process message. Hence, a new handle is created for one-way response.
            ASSERT(outgoingHandle->response == NULL);
            u32 properties = outgoingHandle->properties;
            ASSERT(properties & PERSIST_MSG_PROP);
            //DIST-TODO design: Not sure where to draw the line between one-way with/out ack implementation
            //If the worker was not aware of the no-ack policy, is it ok to always give a handle
            //and the comm-api contract is to at least set the HDL_SEND_OK flag ?
            ocrMsgHandle_t ** sendHandle = ((properties & TWOWAY_MSG_PROP) && !(properties & ASYNC_MSG_PROP))
                ? &outgoingHandle : NULL;
            //DIST-TODO design: who's responsible for deallocating the handle ?
            //If the message is two-way, the creator of the handle is responsible for deallocation
            //If one-way, the comm-layer disposes of the handle when it is not needed anymore
            //=> Sounds like if an ack is expected, caller is responsible for dealloc, else callee
            pd->fcts.sendMessage(pd, outgoingHandle->msg->destLocation, outgoingHandle->msg, sendHandle, properties);

            // This is contractual for now. It recycles the handler allocated in the delegate-comm-api:
            // - Sending a request one-way or a response (always non-blocking): The delegate-comm-api
            //   creates the handle merely to be able to give it to the scheduler. There's no use of the
            //   handle beyond this point.
            // - The runtime does not implement blocking one-way. Hence, the callsite of the original
            //   send message did not ask for a handler to be returned.
            if (sendHandle == NULL) {
                outgoingHandle->destruct(outgoingHandle);
            }

            //Communication is posted. If TWOWAY, subsequent calls to poll may return the response
            //to be processed
            return POLL_MORE_MESSAGE;
        }
    }
    return POLL_NO_MESSAGE;
}

static u8 createProcessRequestEdt(ocrPolicyDomain_t * pd, ocrGuid_t templateGuid, u64 * paramv) {

    ocrGuid_t edtGuid;
    u32 paramc = 1;
    u32 depc = 0;
    u32 properties = 0;
    ocrWorkType_t workType = EDT_RT_WORKTYPE;

    START_PROFILE(api_EdtCreate);
    ocrPolicyMsg_t msg;
    u8 returnCode = 0;
    ocrTask_t *curEdt = NULL;
    getCurrentEnv(NULL, NULL, &curEdt, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_CREATE
    msg.type = PD_MSG_WORK_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = NULL_GUID;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(templateGuid.guid) = templateGuid;
    PD_MSG_FIELD(templateGuid.metaDataPtr) = NULL;
    PD_MSG_FIELD(affinity.guid) = NULL_GUID;
    PD_MSG_FIELD(affinity.metaDataPtr) = NULL;
    PD_MSG_FIELD(outputEvent.guid) = NULL_GUID;
    PD_MSG_FIELD(outputEvent.metaDataPtr) = NULL;
    PD_MSG_FIELD(paramv) = paramv;
    PD_MSG_FIELD(paramc) = paramc;
    PD_MSG_FIELD(depc) = depc;
    PD_MSG_FIELD(depv) = NULL;
    PD_MSG_FIELD(properties) = properties;
    PD_MSG_FIELD(workType) = workType;
    // This is a "fake" EDT so it has no "parent"
    PD_MSG_FIELD(currentEdt.guid) = NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = NULL;
    PD_MSG_FIELD(parentLatch.guid) = NULL_GUID;
    PD_MSG_FIELD(parentLatch.metaDataPtr) = NULL;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode) {
        edtGuid = PD_MSG_FIELD(guid.guid);
        DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Created processRequest EDT GUID 0x%lx\n", edtGuid);
        RETURN_PROFILE(returnCode);
    }

    RETURN_PROFILE(0);
#undef PD_MSG
#undef PD_TYPE
}

//TODO-RL to make the workShift pointer happy
static void workerLoopHcComm_DUMMY() {
    ASSERT(false);
}

static void workerLoopHcCommInternal(ocrWorker_t * worker, ocrGuid_t processRequestTemplate, bool checkEmptyOutgoing) {
    ocrWorkerHc_t * bself = (ocrWorkerHc_t *) worker;
    ocrPolicyDomain_t *pd = worker->pd;

    // Runlevel USER 'communication' loop
    while(bself->workLoopSpin) {
        // First, Ask the scheduler if there are any communication
        // to be scheduled and sent them if any.
        takeFromSchedulerAndSend(pd);

        ocrMsgHandle_t * handle = NULL;
        u8 ret = pd->fcts.pollMessage(pd, &handle);
        if (ret == POLL_MORE_MESSAGE) {
            //IMPL: for now only support successful polls on incoming request and responses
            ASSERT((handle->status == HDL_RESPONSE_OK)||(handle->status == HDL_NORMAL));
            ocrPolicyMsg_t * message = (handle->status == HDL_RESPONSE_OK) ? handle->response : handle->msg;
            //To catch misuses, assert src is not self and dst is self
            ASSERT((message->srcLocation != pd->myLocation) && (message->destLocation == pd->myLocation));
            // Poll a response to a message we had sent.
            if ((message->type & PD_MSG_RESPONSE) && !(handle->properties & ASYNC_MSG_PROP)) {
                DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Received message response for msgId: %ld\n",  message->msgId); // debug
                // Someone is expecting this response, give it back to the PD
                ocrFatGuid_t fatGuid;
                fatGuid.metaDataPtr = handle;
                ocrPolicyMsg_t giveMsg;
                getCurrentEnv(NULL, NULL, NULL, &giveMsg);
            #define PD_MSG (&giveMsg)
            #define PD_TYPE PD_MSG_COMM_GIVE
                giveMsg.type = PD_MSG_COMM_GIVE | PD_MSG_REQUEST;
                PD_MSG_FIELD(guids) = &fatGuid;
                PD_MSG_FIELD(guidCount) = 1;
                PD_MSG_FIELD(properties) = 0;
                PD_MSG_FIELD(type) = OCR_GUID_COMM;
                ret = pd->fcts.processMessage(pd, &giveMsg, false);
                ASSERT(ret == 0);
            #undef PD_MSG
            #undef PD_TYPE
                //For now, assumes all the responses are for workers that are
                //waiting on the response handler provided by sendMessage, reusing
                //the request msg as an input buffer for the response.
            } else {
                ASSERT((message->type & PD_MSG_REQUEST) || ((message->type & PD_MSG_RESPONSE) && (handle->properties & ASYNC_MSG_PROP)));
                // else it's a request or a response with ASYNC_MSG_PROP set (i.e. two-way but asynchronous handling of response).
                DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Received message, msgId: %ld type:0x%x prop:0x%x\n",
                                        message->msgId, message->type, handle->properties);
                // This is an outstanding request, delegate to PD for processing
                u64 msgParamv = (u64) message;
            #ifdef HYBRID_COMM_COMP_WORKER // Experimental see documentation
                // Execute selected 'sterile' messages on the spot
                if ((message->type & PD_MSG_TYPE_ONLY) == PD_MSG_DB_ACQUIRE) {
                    DPRINTF(DEBUG_LVL_VVERB,"hc-comm-worker: Execute message, msgId: %ld\n", pd->myLocation, message->msgId);
                    processRequestEdt(1, &msgParamv, 0, NULL);
                } else {
                    createProcessRequestEdt(pd, processRequestTemplate, &msgParamv);
                }
            #else
                createProcessRequestEdt(pd, processRequestTemplate, &msgParamv);
            #endif
                // We do not need the handle anymore
                handle->destruct(handle);
                //DIST-TODO-3: depending on comm-worker implementation, the received message could
                //then be 'wrapped' in an EDT and pushed to the deque for load-balancing purpose.
            }

        } else {
            //DIST-TODO No messages ready for processing, ask PD for EDT work.
            if (checkEmptyOutgoing && (ret & POLL_NO_OUTGOING_MESSAGE)) {
                break;
            }
        }
    } // run-loop
}

static void runlevel_stop(ocrWorker_t * base) {
    u64 computeCount = base->computeCount;
    u64 i = 0;
    for(i = 0; i < computeCount; i++) {
        base->computes[i]->fcts.stop(base->computes[i], RL_STOP, RL_ACTION_ENTER);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_STOP(base->pd, base->fguid.guid, base->fguid.metaDataPtr,
                         base->computes[i]->fguid.guid,
                         base->computes[i]->fguid.metaDataPtr);
#endif
    }
}

static void hcDistNotifyRunlevelToPd(ocrWorker_t * worker, ocrRunLevel_t newRl, u32 action) {
    ocrPolicyDomain_t * pd;
    ocrPolicyMsg_t msg;
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MGT_RL_NOTIFY
    msg.type = PD_MSG_MGT_RL_NOTIFY | PD_MSG_REQUEST;
    //TODO-RL: for now notify is implicitely meaning the worker reached the runlevel
    PD_MSG_FIELD(runlevel) = newRl;
    PD_MSG_FIELD(action) = action;
    pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
}

//TODO-RL can factorize in hc-worker if allow function pointer registration for each RL
static void workerLoopHcComm(ocrWorker_t * worker) {
    ocrGuid_t processRequestTemplate;
    ocrEdtTemplateCreate(&processRequestTemplate, &processRequestEdt, 1, 0);

    // 'communication' loop: take, send / poll, dispatch, execute
    workerLoopHcCommInternal(worker, processRequestTemplate, false);
    // PRINTF("[%d] WORKER[%d] ENTERING RL_ACTION_QUIESCE_COMM\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    // Received a RL_ACTION_QUIESCE_COMM that broke the loop
    ASSERT(worker->rl == RL_RUNNING_USER_WIP);

    // Try to quiesce communication
    ocrWorkerHc_t * bself = ((ocrWorkerHc_t *) worker);

    // Empty outgoing messages from the scheduler
    ocrPolicyDomain_t *pd = worker->pd;
    while(takeFromSchedulerAndSend(pd) == POLL_MORE_MESSAGE);

    // Loop until pollMessage says there's no more outgoing messages
    // to be processed by the underlying comm-platform
    bself->workLoopSpin = true;
    workerLoopHcCommInternal(worker, processRequestTemplate, true/*check empty queue*/);
    // Here, no more outgoing messages in comm-platform

    // Done with the quiesce comm action
    // PRINTF("[%d] WORKER[%d] COMM DONE RL_ACTION_QUIESCE_COMM\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    bself->workLoopSpin = true; //TODO-RL do we need a barrier here to prevent compiler reordering ?
    hcDistNotifyRunlevelToPd(worker, RL_RUNNING_USER, RL_ACTION_QUIESCE_COMM);
    // Enter the 'keep-working' loop
    // Here we need to keep serving comms because of incoming messages
    // the runtime needs to answer. If we busy-wait, we may deadlock.
    // PRINTF("[%d] WORKER[%d] COMM before loop after comm\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    workerLoopHcCommInternal(worker, processRequestTemplate, false);
    // PRINTF("[%d] WORKER[%d] COMM WORKER stop\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    // Received a RL_ACTION_EXIT that broke the loop

    // PRINTF("[%d] WORKER[%d] COMM hcDistNotifyRunlevelToPd RL_ACTION_EXIT\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    // bself->workLoopSpin = true; //TODO-RL do we need a barrier here to prevent compiler reordering ?
    // When the comm-worker quiesce and it already had all its neighbors PD's shutdown msg
    // we need to make sure there's no outgoing messages pending (i.e. a one-way shutdown) for other PDs
    // before wrapping up the user runlevel
    // Empty outgoing messages from the scheduler
    while(takeFromSchedulerAndSend(pd) == POLL_MORE_MESSAGE);

    // Loop until pollMessage says there's no more outgoing messages
    // to be processed by the underlying comm-platform
    bself->workLoopSpin = true;
    workerLoopHcCommInternal(worker, processRequestTemplate, true/*check empty queue*/);

    hcDistNotifyRunlevelToPd(worker, RL_RUNNING_USER, RL_ACTION_EXIT);

    // Execute the runlevel barrier
    while(worker->rl == RL_RUNNING_USER_WIP);
    // PRINTF("[%d] WORKER[%d] COMM AFTER barrier\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    //
    // RUNLEVEL RT
    //
    ASSERT(worker->rl == RL_RUNNING_RT);

    worker->rl = RL_RUNNING_RT_WIP;
    // Nothing to do in the current distributed implementation
    hcDistNotifyRunlevelToPd(worker, RL_RUNNING_RT, RL_ACTION_EXIT);

    while(worker->rl == RL_RUNNING_RT_WIP);
    ASSERT(worker->rl == RL_STOP);
    // At this point the communication layer is stopped

    runlevel_stop(worker);

    worker->rl = RL_STOP_WIP;
    hcDistNotifyRunlevelToPd(worker, RL_STOP, RL_ACTION_EXIT);

    // PRINTF("[%d] WORKER[%d] COMM exiting thread\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    //
    // This instance of worker becomes inert
    //
}

static bool isBlessedWorker(ocrWorker_t * worker, ocrGuid_t * affinityMasterPD) {
    // Determine if current worker is the master worker of this PD
    bool blessedWorker = (worker->type == MASTER_WORKERTYPE);
    if (blessedWorker) {
        // Determine if current master worker is part of master PD
        u64 count = 0;
        // There should be a single master PD
        ASSERT(!ocrAffinityCount(AFFINITY_PD_MASTER, &count) && (count == 1));
        ocrAffinityGet(AFFINITY_PD_MASTER, &count, affinityMasterPD);
        ASSERT(count == 1);
        blessedWorker &= (worker->pd->myLocation == affinityToLocation(*affinityMasterPD));
    }
    return blessedWorker;
}

void* runWorkerHcComm(ocrWorker_t * worker) {
    ocrGuid_t affinityMasterPD;
    bool blessedWorker = isBlessedWorker(worker, &affinityMasterPD);
    DPRINTF(DEBUG_LVL_INFO,"hc-comm-worker: blessed worker at location %d\n",
            (int)worker->pd->myLocation);
    if (blessedWorker) {
        // This is all part of the mainEdt setup
        // and should be executed by the "blessed" worker.
        void * packedUserArgv = userArgsGet();
        ocrEdt_t mainEdt = mainEdtGet();

        u64 totalLength = ((u64*) packedUserArgv)[0]; // already exclude this first arg
        // strip off the 'totalLength first argument'
        packedUserArgv = (void *) (((u64)packedUserArgv) + sizeof(u64)); // skip first totalLength argument
        ocrGuid_t dbGuid;
        void* dbPtr;
        ocrDbCreate(&dbGuid, &dbPtr, totalLength,
                    DB_PROP_IGNORE_WARN, affinityMasterPD, NO_ALLOC);
        DPRINTF(DEBUG_LVL_INFO,"mainDb guid 0x%lx ptr %p\n", dbGuid, dbPtr);
        // copy packed args to DB
        hal_memCopy(dbPtr, packedUserArgv, totalLength, 0);

        // Prepare the mainEdt for scheduling
        ocrGuid_t edtTemplateGuid, edtGuid;
        ocrEdtTemplateCreate(&edtTemplateGuid, mainEdt, 0, 1);
        ocrEdtCreate(&edtGuid, edtTemplateGuid, EDT_PARAM_DEF, /* paramv = */ NULL,
                    /* depc = */ EDT_PARAM_DEF, /* depv = */ &dbGuid,
                    EDT_PROP_NONE, affinityMasterPD, NULL);
    } else {
        // Set who we are
        ocrPolicyDomain_t *pd = worker->pd;
        u32 i;
        for(i = 0; i < worker->computeCount; ++i) {
            worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);
        }
    }

    DPRINTF(DEBUG_LVL_INFO, "Starting scheduler routine of worker %ld\n", ((ocrWorkerHc_t *) worker)->id);
    workerLoopHcComm(worker);
    return NULL;
}

void hcDistStopWorker(ocrWorker_t * base, ocrRunLevel_t rl, u32 actionRl) {
    if (actionRl == RL_ACTION_ENTER) {
        // delegate to base
        ocrWorkerHcComm_t * derived = (ocrWorkerHcComm_t *) base;
        derived->baseStop(base, rl, actionRl);
    } else {
        switch(rl) {
            case RL_RUNNING_USER: {
                ASSERT((base->rl == RL_RUNNING_USER) || (base->rl == RL_RUNNING_USER_WIP));
                if (actionRl == RL_ACTION_QUIESCE_COMM) {
                    // Make the worker exit its current work loop
                    base->rl = RL_RUNNING_USER_WIP;
                    ocrWorkerHc_t * bself = (ocrWorkerHc_t *) base;
                    // Break the 'communication' loop
                    bself->workLoopSpin = false;
                } else if (actionRl == RL_ACTION_EXIT) {
                    // Break the 'keep working' loop
                    ocrWorkerHc_t * bself = (ocrWorkerHc_t *) base;
                    bself->workLoopSpin = false;
                    // PRINTF("[%d] stop RL_ACTION_EXIT on COMM\n", (int)base->pd->myLocation);
                } else {
                    ASSERT(actionRl != 0);
                    // nothing to do for other actions, just notify it went through
                    hcDistNotifyRunlevelToPd(base, RL_RUNNING_USER, actionRl);
                }
                break;
            }
            case RL_RUNNING_RT: {
                //TODO-RL check these WIP conditions everywhere
                ASSERT((base->rl == RL_RUNNING_RT) || (base->rl == RL_RUNNING_RT_WIP));
                ASSERT(actionRl == RL_ACTION_EXIT);
                base->rl = RL_RUNNING_RT_WIP;
                break;
            }
            case RL_STOP: {
                ASSERT(actionRl == RL_ACTION_EXIT);
                // Nothing to do
                //TODO-RL do we need this ?
                break;
            }
            default:
                ASSERT("No implementation to enter runlevel");
        }
    }
}

/**
 * Builds an instance of a HC Communication worker
 */
ocrWorker_t* newWorkerHcComm(ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * worker = (ocrWorker_t*)
            runtimeChunkAlloc(sizeof(ocrWorkerHcComm_t), NULL);
    factory->initialize(factory, worker, perInstance);
    return (ocrWorker_t *) worker;
}

void initializeWorkerHcComm(ocrWorkerFactory_t * factory, ocrWorker_t *self, ocrParamList_t *perInstance) {
    ocrWorkerFactoryHcComm_t * derivedFactory = (ocrWorkerFactoryHcComm_t *) factory;
    derivedFactory->baseInitialize(factory, self, perInstance);
    // Override base's default value
    ocrWorkerHc_t * workerHc = (ocrWorkerHc_t *) self;
    workerHc->hcType = HC_WORKER_COMM;
    // Initialize comm worker's members
    ocrWorkerHcComm_t * workerHcComm = (ocrWorkerHcComm_t *) self;
    workerHcComm->baseStop = derivedFactory->baseStop;
}

/******************************************************/
/* OCR-HC COMMUNICATION WORKER FACTORY                */
/******************************************************/

void destructWorkerFactoryHcComm(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryHcComm(ocrParamList_t * perType) {
    ocrWorkerFactory_t * baseFactory = newOcrWorkerFactoryHc(perType);
    ocrWorkerFcts_t baseFcts = baseFactory->workerFcts;

    ocrWorkerFactoryHcComm_t* derived = (ocrWorkerFactoryHcComm_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryHcComm_t), (void *)1);
    ocrWorkerFactory_t * base = (ocrWorkerFactory_t *) derived;
    base->instantiate = FUNC_ADDR(ocrWorker_t* (*)(ocrWorkerFactory_t*, ocrParamList_t*), newWorkerHcComm);
    base->initialize  = FUNC_ADDR(void (*)(ocrWorkerFactory_t*, ocrWorker_t*, ocrParamList_t*), initializeWorkerHcComm);
    base->destruct    = FUNC_ADDR(void (*)(ocrWorkerFactory_t*), destructWorkerFactoryHcComm);

    // Copy base's function pointers
    base->workerFcts = baseFcts;
    derived->baseInitialize = baseFactory->initialize;
    derived->baseStop = baseFcts.stop;

    // Specialize comm functions
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), runWorkerHcComm);
    base->workerFcts.stop = FUNC_ADDR(void (*) (ocrWorker_t *,ocrRunLevel_t,u32), hcDistStopWorker);
    //TODO-RL: This doesn't really work out for communication-workers
    base->workerFcts.workShift = FUNC_ADDR(void* (*) (ocrWorker_t *), workerLoopHcComm_DUMMY);

    baseFactory->destruct(baseFactory);
    return base;
}

#endif /* ENABLE_WORKER_HC_COMM */
