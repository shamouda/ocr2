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

    DPRINTF(DEBUG_LVL_VERB, "Starting scheduler routine of CE worker %ld\n", getWorkerId(worker));
    while(worker->fcts.isRunning(worker)) {
        ocrMsgHandle_t *handle = NULL;
        RESULT_ASSERT(pd->fcts.waitMessage(pd, &handle), ==, 0);
        ASSERT(handle);
        ocrPolicyMsg_t *msg = handle->response;
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
    workerCe->id = ((paramListWorkerCeInst_t*)perInstance)->workerId;
    workerCe->running = false;
    workerCe->secondStart = false;
}


u8 ceWorkerSwitchRunlevel(ocrWorker_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                          u32 phase, u32 properties, void (*callback)(ocrPolicyDomain_t *, u64), u64 val) {

    u8 toReturn = 0;

    // Verify properties
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    // Call the runlevel change on the underlying platform
    if(runlevel == RL_CONFIG_PARSE && (properties & RL_BRING_UP) && phase == 0) {
        // Set the worker properly the first time
        ASSERT(self->computeCount == 1);
        self->computes[0]->worker = self;
    }
    // Even if we have a callback, we make things synchronous for the computes
    if(runlevel != RL_COMPUTE_OK) {
        // For compute OK, we need to do things BEFORE calling this
        toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                           NULL, 0);
    }
#if 0
    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
#ifdef DIST_SUPPORT
                // We need at least three phases for the RL_USER_OK TEAR_DOWN
                RL_ENSURE_PHASE_DOWN(PD, RL_USER_OK, RL_PHASE_WORKER, 3);
#endif
                // We need at least two phases for the RL_COMPUTE_OK TEAR_DOWN
                RL_ENSURE_PHASE_DOWN(PD, RL_COMPUTE_OK, RL_PHASE_WORKER, 2);
            } else if(RL_IS_LAST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // We check that the compute and user phases have the right
                // count. We currently only support one user phase and two
                // compute phase. If this changes, the workerLoop code and hcWorkerRun
                // code will have to be modified (as well as this code of course)
#ifdef DIST_SUPPORT
                if(RL_GET_PHASE_COUNT_UP(PD, RL_COMPUTE_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_COMPUTE_OK) != 2 ||
                   RL_GET_PHASE_COUNT_UP(PD, RL_USER_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK) != 3) {
                    DPRINTF(DEBUG_LVL_WARN, "Worker does not support compute and user counts\n");
                    ASSERT(0);
                }
#else
                if(RL_GET_PHASE_COUNT_UP(PD, RL_COMPUTE_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_COMPUTE_OK) != 2 ||
                   RL_GET_PHASE_COUNT_UP(PD, RL_USER_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK) != 1) {
                    DPRINTF(DEBUG_LVL_WARN, "Worker does not support compute and user counts\n");
                    ASSERT(0);
                }
#endif
            }
        }
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP)
            self->pd = PD;
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
            // Guidify ourself
            guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_WORKER);
            // We need a way to inform the PD
            ASSERT(callback != NULL);
            self->curState = RL_MEMORY_OK << 16; // Technically last phase of memory OK but doesn't really matter
            self->desiredState = RL_COMPUTE_OK << 16 | phase;

            // See if we are blessed
#ifdef DIST_SUPPORT
            ocrGuid_t affinityMasterPD;
            u64 count = 0;
            // There should be a single master PD
            ASSERT(!ocrAffinityCount(AFFINITY_PD_MASTER, &count) && (count == 1));
            ocrAffinityGet(AFFINITY_PD_MASTER, &count, &affinityMasterPD);
            self->amBlessed = ((properties & RL_BLESSED) != 0) & (self->pd->myLocation == affinityToLocation(affinityMasterPD));
#else
            self->amBlessed = (properties & RL_BLESSED) != 0;
#endif
            if(!(properties & RL_PD_MASTER)) {
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                                   NULL, 0);
            } else {
                // First set our current environment (this is usually done by the new thread's startup code)
                self->computes[0]->fcts.setCurrentEnv(self->computes[0], self->pd, self);
                // We just directly call the callback after switching our underlying target
                toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                                   NULL, 0);
                callback(self->pd, val);
                self->curState = RL_COMPUTE_OK << 16;
            }

        }
        if((properties & RL_TEAR_DOWN)) {
            toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                               NULL, 0);
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
                // At this stage, only the RL_PD_MASTER should be actually
                // capable
                DPRINTF(DEBUG_LVL_VERB, "Last phase in RL_COMPUTE_OK DOWN for 0x%llx (am PD master: %d)\n",
                    self, properties & RL_PD_MASTER);
                self->desiredState = self->curState = (RL_COMPUTE_OK << 16) | phase;
            } else if(RL_IS_FIRST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                ASSERT(self->curState == (RL_USER_OK << 16));
                ASSERT(callback != NULL);
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                self->desiredState = RL_COMPUTE_OK << 16 | phase;
            } else {
                ASSERT(false && "Unexpected phase on runlevel RL_COMPUTE_OK teardown");
            }
        }
        break;
    case RL_USER_OK:
        if((properties & RL_BRING_UP)) {
            if(RL_IS_LAST_PHASE_UP(PD, RL_USER_OK, phase)) {
                if(!(properties & RL_PD_MASTER)) {
                    // No callback required on the bring-up
                    self->callback = NULL;
                    self->callbackArg = 0ULL;
                    hal_fence();
                    self->desiredState = (RL_USER_OK << 16) | RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK); // We put ourself one past
                    // so that we can then come back down when
                    // shutting down
                } else {
                    // At this point, the original capable thread goes to work
                    self->curState = self->desiredState = ((RL_USER_OK << 16) | RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK));
                    workerLoop(self);
                }
            }
        }
#ifdef DIST_SUPPORT
        u32 PHASE_RUN = 3;
        u32 PHASE_COMP_QUIESCE = 2;
        u32 PHASE_COMM_QUIESCE = 1;
        u32 PHASE_DONE = 0;
        //Ignore other RL
        if (properties & RL_TEAR_DOWN) {
            if (phase == PHASE_COMP_QUIESCE) {
                // We make sure that we actually fully booted before shutting down.
                // Addresses a race where a worker still hasn't started but
                // another worker has started and executes the shutdown protocol
                while(self->curState != ((RL_USER_OK << 16) | (phase + 1)));
                ASSERT(self->curState == ((RL_USER_OK << 16) | (phase + 1)));

                // Transition from RUN to PHASE_COMP_QUIESCE
                ASSERT((self->curState & 0xFFFF) == PHASE_RUN);
                ASSERT(callback != NULL);
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                // Breaks the worker's compute loop
                self->desiredState = RL_USER_OK << 16 | PHASE_COMP_QUIESCE;
            }
            if (phase == PHASE_COMM_QUIESCE) {
                // Transition to PHASE_COMM_QUIESCE
                ASSERT((self->curState & 0xFFFF) == (phase+1));
                ASSERT(callback != NULL);
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                // Breaks the worker's compute loop
                self->desiredState = RL_USER_OK << 16 | PHASE_COMM_QUIESCE;
            }
            if (RL_IS_LAST_PHASE_DOWN(PD, RL_USER_OK, phase)) {
                ASSERT((self->curState & 0xFFFF) == (phase+1));
                ASSERT(callback != NULL);
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                // Breaks the worker's compute loop
                self->desiredState = RL_USER_OK << 16 | PHASE_DONE;
            }
        }
#else
        if((properties & RL_TEAR_DOWN) && RL_IS_FIRST_PHASE_DOWN(PD, RL_USER_OK, phase)) {
            // We need to break out of the compute loop
            // We need to have a callback for all workers here
            ASSERT(callback != NULL);
            // We make sure that we actually fully booted before shutting down.
            // Addresses a race where a worker still hasn't started but
            // another worker has started and executes the shutdown protocol
            while(self->curState != ((RL_USER_OK << 16) | (phase + 1))) ;
            ASSERT(self->curState == ((RL_USER_OK << 16) | (phase + 1)));
            self->callback = callback;
            self->callbackArg = val;
            hal_fence();
            // Breaks the worker's compute loop
            self->desiredState = (RL_USER_OK << 16) | phase;
        }
#endif
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
#endif
    return toReturn;
}

/*
void ceBeginWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {

    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;

    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; ++i) {
        base->computes[i]->fcts.begin(base->computes[i], policy, base->type);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
        base->computes[i]->fcts.setCurrentEnv(base->computes[i], policy, base);
    }
}

void ceStartWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {

    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;

    DPRINTF(DEBUG_LVL_VVERB, "Starting worker\n");
    //TODO-RL check if RL fixes this scenario
#ifdef ENABLE_COMP_PLATFORM_PTHREAD  //FIXME: Trac bugs #76 and #80
    if(base->type == MASTER_WORKERTYPE && !ceWorker->secondStart) {
        ceWorker->secondStart = true;
        return; // Don't start right away
    }
#endif

    base->location = policy->myLocation;
    // Get a GUID
    guidify(policy, (u64)base, &(base->fguid), OCR_GUID_WORKER);
    base->pd = policy;

    ceWorker->running = true;

    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;
    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; i++) {
        base->computes[i]->fcts.start(base->computes[i], policy, base);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
    }
}
*/
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
/*
void ceStopWorker(ocrWorker_t * base, ocrRunLevel_t newRl) {
    switch(newRl) {
        case RL_STOP: {
            ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;
            ceWorker->running = false;

            u64 computeCount = base->computeCount;
            u64 i = 0;
            // This code should be called by the master thread to join others
            for(i = 0; i < computeCount; i++) {
                base->computes[i]->fcts.stop(base->computes[i], newRl);
        #ifdef OCR_ENABLE_STATISTICS
                statsWORKER_STOP(base->pd, base->fguid.guid, base->fguid.metaDataPtr,
                                 base->computes[i]->fguid.guid,
                                 base->computes[i]->fguid.metaDataPtr);
        #endif
            }
            DPRINTF(DEBUG_LVL_INFO, "Stopping worker %ld\n", getWorkerId(base));

            // Destroy the GUID
            PD_MSG_STACK(msg);
            getCurrentEnv(NULL, NULL, NULL, &msg);

        #define PD_MSG (&msg)
        #define PD_TYPE PD_MSG_GUID_DESTROY
            msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = base->fguid;
            PD_MSG_FIELD_I(properties) = 0;
            RESULT_ASSERT(base->pd->fcts.processMessage(base->pd, &msg, false), ==, 0);
        #undef PD_MSG
        #undef PD_TYPE
            base->fguid.guid = UNINITIALIZED_GUID;
            break;
        }
        case RL_SHUTDOWN: {
            DPRINTF(DEBUG_LVL_INFO, "Finishing worker routine %ld\n", getWorkerId(base));
            ASSERT(base->computeCount == 1);
            u64 i = 0;
            for(i = 0; i < base->computeCount; i++) {
                base->computes[i]->fcts.stop(base->computes[i], RL_SHUTDOWN);
            }
            break;
        }
        default:
            ASSERT("Unknown runlevel in stop function");
    }
}
*/
bool ceIsRunningWorker(ocrWorker_t * base) {
    ocrWorkerCe_t * ceWorker = (ocrWorkerCe_t *) base;
    return ceWorker->running;
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
                                                       u32, u32, void (*)(ocrPolicyDomain_t*, u64), u64), ceWorkerSwitchRunlevel);
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), ceRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*)(ocrWorker_t*), ceWorkShift);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*)(ocrWorker_t*), ceIsRunningWorker);
    return base;
}

#endif /* ENABLE_WORKER_CE */
