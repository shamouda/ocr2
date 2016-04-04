/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_CE

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-errors.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "worker/ce/ce-worker.h"
#include "policy-domain/ce/ce-policy.h"

#ifdef HAL_FSIM_CE
#include "xstg-map.h"
#endif

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-CE WORKER                                      */
/******************************************************/

// Convenient to have an id to index workers in pools
static inline u64 getWorkerId(ocrWorker_t * worker) {
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) worker;
    return ceWorker->id;
}

/**
 * The computation worker routine that asks for work to the scheduler
 */
static void workerLoop(ocrWorker_t * worker) {
    ocrPolicyDomain_t *pd = worker->pd;
    PD_MSG_STACK(umsg);
    getCurrentEnv(NULL, NULL, NULL, &umsg);

    DPRINTF(DEBUG_LVL_VERB, "Starting scheduler routine of CE worker %"PRId64"\n", getWorkerId(worker));
    ocrMsgHandle_t handle;
    ocrMsgHandle_t *pHandle = &handle;
    while(worker->fcts.isRunning(worker)) {
        DPRINTF(DEBUG_LVL_VVERB, "UPDATE IDLE\n");
#define PD_MSG (&umsg)
#define PD_TYPE PD_MSG_SCHED_UPDATE
        umsg.type = PD_MSG_SCHED_UPDATE | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(properties) = OCR_SCHEDULER_UPDATE_PROP_IDLE;
        RESULT_ASSERT(pd->fcts.processMessage(pd, &umsg, false), ==, 0);
        ASSERT(PD_MSG_FIELD_O(returnDetail) == 0);
#undef PD_MSG
#undef PD_TYPE

        DPRINTF(DEBUG_LVL_VVERB, "WAIT\n");
        pd->commApis[0]->fcts.initHandle(pd->commApis[0], pHandle);
        RESULT_ASSERT(pd->fcts.waitMessage(pd, &pHandle), ==, 0);
        ASSERT(pHandle);
        ocrPolicyMsg_t *msg = pHandle->response;
        RESULT_ASSERT(pd->fcts.processMessage(pd, msg, true), ==, 0);
        pHandle->destruct(pHandle);
    } /* End of while loop */
    // At this stage, when we drop out, we should be
    // out of USER_OK
}

void destructWorkerCe(ocrWorker_t * base) {
    u64 i = 0;
    // There is only one compute for now but we
    // keep the generality
    while(i < base->computeCount) {
        base->computes[i]->fcts.destruct(base->computes[i]);
        ++i;
    }
    runtimeChunkFree((u64)(base->computes), NULL);
    runtimeChunkFree((u64)base, NULL);
}

/**
 * Builds an instance of a CE worker
 */
ocrWorker_t* newWorkerCe (ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * base = (ocrWorker_t*)runtimeChunkAlloc(sizeof(ocrWorkerCe_t), PERSISTENT_CHUNK);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeWorkerCe(ocrWorkerFactory_t * factory, ocrWorker_t* base, ocrParamList_t * perInstance) {
    initializeWorkerOcr(factory, base, perInstance);
    base->type = ((paramListWorkerCeInst_t*)perInstance)->workerType;
    ocrWorkerCe_t * workerCe = (ocrWorkerCe_t *) base;
    workerCe->id = ((paramListWorkerInst_t*)perInstance)->workerId;
}


u8 ceWorkerSwitchRunlevel(ocrWorker_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                          phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t *, u64), u64 val) {

    u8 toReturn = 0;

    // Verify properties
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));
    ASSERT(properties & RL_PD_MASTER); // One worker per PD.
    ASSERT(callback == NULL); // This worker does not support callbacks

    // Call the runlevel change on the underlying platform
    switch (runlevel) {
    case RL_CONFIG_PARSE:
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            // Set the worker properly the first time
            ASSERT(self->computeCount == 1);
            self->computes[0]->worker = self;
            self->pd = PD;
            self->location = PD->myLocation;
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
            // Guidify ourself
            guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_WORKER);
            // Set that we want to run. We only use the RL and consider RL_USER_OK to be running and
            // anything else to be not running
            self->curState = GET_STATE(RL_USER_OK, 0);
            if(properties & RL_PD_MASTER) {
                // We need to set our environment
                self->computes[0]->fcts.setCurrentEnv(self->computes[0], self->pd, self);
            }
            break;
        }
        if(properties & RL_TEAR_DOWN) {
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                // Destroy GUID
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
                msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(guid) = self->fguid;
                PD_MSG_FIELD_I(properties) = 0;
                toReturn |= self->pd->fcts.processMessage(self->pd, &msg, false);
                self->fguid.guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
            }
        }
        break;
    case RL_USER_OK:
        if((properties & RL_BRING_UP) && (RL_IS_LAST_PHASE_UP(PD, RL_USER_OK, phase))) {
            if(properties & RL_PD_MASTER) {
                // We start ourself
                self->fcts.run(self);
            }
        } else if((properties & RL_TEAR_DOWN) && (RL_IS_LAST_PHASE_DOWN(PD, RL_USER_OK, phase))) {
            // Break out of the loop. This will return us to the RL_USER_OK RL switch
            // code in the PD so that we can continue shutdown. This works because there is
            // only one worker per PD
            self->curState = GET_STATE(RL_COMPUTE_OK, 0);
        }
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }

    toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                       callback, val);
    return toReturn;
}

void* ceRunWorker(ocrWorker_t * worker) {
    // Need to pass down a data-structure
    ocrPolicyDomain_t *pd = worker->pd;

    // Set who we are
    u32 i;
    for(i = 0; i < worker->computeCount; ++i) {
        worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);
    }

    workerLoop(worker);
    return NULL;
}

void* ceWorkShift(ocrWorker_t * worker) {
    ASSERT(0); // Not supported
    return NULL;
}

bool ceIsRunningWorker(ocrWorker_t * base) {
    return GET_STATE_RL(base->curState) == RL_USER_OK;
}

void cePrintLocation(ocrWorker_t *base, char* location) {
    // BUG #605: Make the notion of location more robust. This should be made
    // more platform agnostic
#ifdef HAL_FSIM_CE
    SNPRINTF(location, 32, "CE %"PRId64" Block %"PRId64" Cluster %"PRId64"", AGENT_FROM_ID(base->location),
             BLOCK_FROM_ID(base->location), CLUSTER_FROM_ID(base->location));
#else
    SNPRINTF(location, 32, "CE");
#endif
}

/******************************************************/
/* OCR-CE WORKER FACTORY                              */
/******************************************************/

void destructWorkerFactoryCe(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryCe(ocrParamList_t * perType) {
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryCe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newWorkerCe;
    base->initialize = &initializeWorkerCe;
    base->destruct = &destructWorkerFactoryCe;

    base->workerFcts.destruct = FUNC_ADDR(void (*)(ocrWorker_t*), destructWorkerCe);
    base->workerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrWorker_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                       phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), ceWorkerSwitchRunlevel);
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), ceRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*)(ocrWorker_t*), ceWorkShift);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*)(ocrWorker_t*), ceIsRunningWorker);
    base->workerFcts.printLocation = FUNC_ADDR(void (*)(ocrWorker_t*, char* location), cePrintLocation);
    return base;
}

#endif /* ENABLE_WORKER_CE */
