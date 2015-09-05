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
#include "rmd-map.h"
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
    ocrPolicyDomainCe_t * cePolicy = (ocrPolicyDomainCe_t *)pd;
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);

    DPRINTF(DEBUG_LVL_VERB, "Starting scheduler routine of CE worker %ld\n", getWorkerId(worker));
    pd->fcts.switchRunlevel(pd, RL_COMPUTE_OK, 0);
    while(worker->fcts.isRunning(worker)) {
        if (cePolicy->shutdownMode) {
            DPRINTF(DEBUG_LVL_VVERB, "UPDATE SHUTDOWN\n");
        } else {
            DPRINTF(DEBUG_LVL_VVERB, "UPDATE IDLE\n");
        }
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_SCHED_UPDATE
        msg.type = PD_MSG_SCHED_UPDATE | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(properties) = cePolicy->shutdownMode ? OCR_SCHEDULER_UPDATE_PROP_SHUTDOWN : OCR_SCHEDULER_UPDATE_PROP_IDLE;
        ASSERT(pd->fcts.processMessage(pd, &msg, false) == 0);
        ASSERT(PD_MSG_FIELD_O(returnDetail) == 0);
#undef PD_MSG
#undef PD_TYPE

        DPRINTF(DEBUG_LVL_VVERB, "WAIT\n");
        ocrMsgHandle_t *handle = NULL;
        RESULT_ASSERT(pd->fcts.waitMessage(pd, &handle), ==, 0);
        ASSERT(handle);
        ocrPolicyMsg_t *msg = handle->response;
        DPRINTF(DEBUG_LVL_VVERB, "PROCESSING: %lx from %lx\n", (msg->type & PD_MSG_TYPE_ONLY), msg->srcLocation);
        RESULT_ASSERT(pd->fcts.processMessage(pd, msg, true), ==, 0);
        handle->destruct(handle);
    } /* End of while loop */

}

void destructWorkerCe(ocrWorker_t * base) {
    u64 i = 0;
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
    workerCe->running = false;
}


u8 ceWorkerSwitchRunlevel(ocrWorker_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                          phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t *, u64), u64 val) {

    u8 toReturn = 0;

    // Verify properties
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    // Call the runlevel change on the underlying platform
    switch (runlevel) {
        case RL_PD_OK:
            // Set the worker properly the first time
            ASSERT(self->computeCount == 1);
            self->computes[0]->worker = self;
            self->pd = PD;
            break;
        case RL_COMPUTE_OK:
            self->location = PD->myLocation;
            self->pd = PD;
            ((ocrWorkerCe_t *) self)->running = true;
            break;
        case RL_USER_OK:
            ((ocrWorkerCe_t *) self)->running = false;
            break;
        default:
            break;
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
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;
    return ceWorker->running;
}

void cePrintLocation(ocrWorker_t *base, char* location) {
    // BUG #605: Make the notion of location more robust. This should be made
    // more platform agnostic
#ifdef HAL_FSIM_CE
    SNPRINTF(location, 32, "CE %d Block %d Unit %d", AGENT_FROM_ID(base->location),
             BLOCK_FROM_ID(base->location), UNIT_FROM_ID(base->location));
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
