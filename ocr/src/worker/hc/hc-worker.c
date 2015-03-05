/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_HC

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-db.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "worker/hc/hc-worker.h"

#include "experimental/ocr-placer.h"
#include "extensions/ocr-affinity.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#ifdef OCR_RUNTIME_PROFILER
#include "utils/profiler/profiler.h"
#endif

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-HC WORKER                                      */
/******************************************************/

// Convenient to have an id to index workers in pools
static inline u64 getWorkerId(ocrWorker_t * worker) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    return hcWorker->id;
}

static void hcWorkShift(ocrWorker_t * worker) {
    ocrPolicyDomain_t * pd;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
    ocrFatGuid_t taskGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
    u32 count = 1;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_COMM_TAKE
    msg.type = PD_MSG_COMM_TAKE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guids) = &taskGuid;
    PD_MSG_FIELD_IO(guidCount) = count;
    PD_MSG_FIELD_I(properties) = 0;
    PD_MSG_FIELD_IO(type) = OCR_GUID_EDT;
    // TODO: In the future, potentially take more than one)
    if(pd->fcts.processMessage(pd, &msg, true) == 0) {
        // We got a response
        count = PD_MSG_FIELD_IO(guidCount);
        if(count == 1) {
            // Stolen task sanity checks
            ASSERT(taskGuid.guid != NULL_GUID && taskGuid.metaDataPtr != NULL);
            worker->curTask = (ocrTask_t*)taskGuid.metaDataPtr;
            DPRINTF(DEBUG_LVL_VERB, "Worker shifting to execute EDT GUID 0x%lx\n", taskGuid.guid);
            u8 (*executeFunc)(ocrTask_t *) = (u8 (*)(ocrTask_t*))PD_MSG_FIELD_IO(extra); // Execute is stored in extra
            executeFunc(worker->curTask);
#undef PD_MSG
#undef PD_TYPE
            // Destroy the work
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_DESTROY
            getCurrentEnv(NULL, NULL, NULL, &msg);
            msg.type = PD_MSG_WORK_DESTROY | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = taskGuid;
            PD_MSG_FIELD_I(currentEdt) = taskGuid;
            PD_MSG_FIELD_I(properties) = 0;
            // Ignore failures, we may be shutting down
            pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
            // Important for this to be the last
            worker->curTask = NULL;
        }
    }
}

static void workerLoop(ocrWorker_t * worker) {
    u8 continueLoop = true;
    // At this stage, we are in the USER_OK runlevel
    ASSERT(worker->curState == GET_STATE(RL_USER_OK, (RL_GET_PHASE_COUNT_DOWN(worker->pd, RL_USER_OK))));

    if (worker->amBlessed) {
        ocrGuid_t affinityMasterPD;
        u64 count = 0;
        // There should be a single master PD
        ASSERT(!ocrAffinityCount(AFFINITY_PD_MASTER, &count) && (count == 1));
        ocrAffinityGet(AFFINITY_PD_MASTER, &count, &affinityMasterPD);

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
        // copy packed args to DB
        hal_memCopy(dbPtr, packedUserArgv, totalLength, 0);
        // Prepare the mainEdt for scheduling
        ocrGuid_t edtTemplateGuid = NULL_GUID, edtGuid = NULL_GUID;
        ocrEdtTemplateCreate(&edtTemplateGuid, mainEdt, 0, 1);
        ocrEdtCreate(&edtGuid, edtTemplateGuid, EDT_PARAM_DEF, /* paramv = */ NULL,
                     /* depc = */ EDT_PARAM_DEF, /* depv = */ &dbGuid,
                     EDT_PROP_NONE, affinityMasterPD, NULL);
    }

    // Actual loop
    do {
        while(worker->curState == worker->desiredState) {
            START_PROFILE(wo_hc_workerLoop);
            worker->fcts.workShift(worker);
            EXIT_PROFILE;
        }
        // Here we are shifting to another runlevel or phase

        switch(GET_STATE_RL(worker->desiredState)) {
        case RL_USER_OK: {
            u8 desiredPhase = GET_STATE_PHASE(worker->desiredState);
            // Should never fall-through here if there has been no transition
            ASSERT(desiredPhase != RL_GET_PHASE_COUNT_DOWN(worker->pd, RL_USER_OK));
            ASSERT(worker->callback != NULL);
            worker->curState = GET_STATE(RL_USER_OK, desiredPhase);
            // Callback the PD, but keep working
            worker->callback(worker->pd, worker->callbackArg);
            // Warning: Code potentially concurrent with switchRunlevel
            break;
        }
        case RL_COMPUTE_OK: {
            u8 phase = GET_STATE_PHASE(worker->desiredState);
            if(RL_IS_FIRST_PHASE_DOWN(worker->pd, RL_COMPUTE_OK, phase)) {
                DPRINTF(DEBUG_LVL_VERB, "Noticed transition to RL_COMPUTE_OK\n");

                // We first change our state prior to the callback
                // because we may end up doing some of the callback processing
                worker->curState = worker->desiredState;
                if(worker->callback != NULL) {
                    worker->callback(worker->pd, worker->callbackArg);
                }
                // There is no need to do anything else except quit
                continueLoop = false;
            } else {
                ASSERT(0);
            }
            break;
        }
        default:
            // Only these two RL should occur
            ASSERT(0);
        }
    } while(continueLoop);

    DPRINTF(DEBUG_LVL_VERB, "Finished worker loop ... waiting to be reapped\n");
}

void destructWorkerHc(ocrWorker_t * base) {
}

u8 hcWorkerSwitchRunlevel(ocrWorker_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                          phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t *, u64), u64 val) {

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

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // This worker implementation supports an arbitrary number
                // of down phases in RL_USER_OK. Each phase goes into the work loop.
                // We need at least two phases for the RL_COMPUTE_OK TEAR_DOWN
                RL_ENSURE_PHASE_DOWN(PD, RL_COMPUTE_OK, RL_PHASE_WORKER, 2);
            } else if(RL_IS_LAST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // We check that the compute and user phases have the right
                // count. We currently only support one user phase and two
                // compute phase. If this changes, the workerLoop code and hcWorkerRun
                // code will have to be modified (as well as this code of course)
                if(RL_GET_PHASE_COUNT_UP(PD, RL_COMPUTE_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_COMPUTE_OK) != 2 ||
                   RL_GET_PHASE_COUNT_UP(PD, RL_USER_OK) != 1 ||
                   RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK) != 1) {
                    DPRINTF(DEBUG_LVL_WARN, "Worker does not support compute and user counts\n");
                    ASSERT(0);
                }
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
            self->curState = GET_STATE(RL_MEMORY_OK, 0); // Technically last phase of memory OK but doesn't really matter
            self->desiredState = GET_STATE(RL_COMPUTE_OK, phase);

            // See if we are blessed
            self->amBlessed = (properties & RL_BLESSED) != 0;

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
                self->curState = GET_STATE(RL_COMPUTE_OK, 0);
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
                self->desiredState = self->curState = GET_STATE(RL_COMPUTE_OK, phase);
            } else if(RL_IS_FIRST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                ASSERT(self->curState == GET_STATE(RL_USER_OK, 0));
                ASSERT(callback != NULL);
                self->callback = callback;
                self->callbackArg = val;
                hal_fence();
                self->desiredState = GET_STATE(RL_COMPUTE_OK, phase);
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
                    self->desiredState = GET_STATE(RL_USER_OK, (RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK))); // We put ourself one past
                    // so that we can then come back down when
                    // shutting down
                } else {
                    // At this point, the original capable thread goes to work
                    self->curState = self->desiredState = GET_STATE(RL_USER_OK, (RL_GET_PHASE_COUNT_DOWN(PD, RL_USER_OK)));
                    workerLoop(self);
                }
            }
        }
        if (properties & RL_TEAR_DOWN) {
            if (RL_IS_FIRST_PHASE_DOWN(PD, RL_USER_OK, phase)) {
                // We make sure that we actually fully booted before shutting down.
                // Addresses a race where a worker still hasn't started but
                // another worker has started and executes the shutdown protocol
                while(self->curState != GET_STATE(RL_USER_OK, (phase+1)));
                ASSERT(self->curState == GET_STATE(RL_USER_OK, (phase+1)));
            }

            // Transition to the next phase
            ASSERT(GET_STATE_PHASE(self->curState) == (phase+1));
            ASSERT(callback != NULL);
            self->callback = callback;
            self->callbackArg = val;
            hal_fence();
            // Breaks the worker's compute loop
            self->desiredState = GET_STATE(RL_USER_OK, phase);
        }
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

void* hcRunWorker(ocrWorker_t * worker) {
    // TODO: This probably needs to go away and be set directly
    // by the PD during one of the RLs
    //Register this worker and get a context id
    ocrPolicyDomain_t *pd = worker->pd;
    PD_MSG_STACK(msg);
    msg.srcLocation = pd->myLocation;
    msg.destLocation = pd->myLocation;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MGT_REGISTER
    msg.type = PD_MSG_MGT_REGISTER | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(loc) = (ocrLocation_t)getWorkerId(worker);
    PD_MSG_FIELD_I(properties) = 0;
    pd->fcts.processMessage(pd, &msg, true);
    worker->seqId = PD_MSG_FIELD_O(seqId);
#undef PD_MSG
#undef PD_TYPE

    // At this point, we should have a callback to inform the PD
    // that we have successfully achieved the RL_COMPUTE_OK RL
    ASSERT(worker->callback != NULL);
    worker->callback(worker->pd, worker->callbackArg);

    // Set the current environment
    worker->computes[0]->fcts.setCurrentEnv(worker->computes[0], worker->pd, worker);
    worker->curState = GET_STATE(RL_COMPUTE_OK, 0);

    // We wait until we transition to the next RL
    while(worker->curState == worker->desiredState) ;

    // At this point, we should be going to RL_USER_OK
    ASSERT(worker->desiredState == GET_STATE(RL_USER_OK, (RL_GET_PHASE_COUNT_DOWN(worker->pd, RL_USER_OK))));

    // Start the worker loop
    worker->curState = worker->desiredState;
    workerLoop(worker);
    // Worker loop will transition back down to RL_COMPUTE_OK last phase

    ASSERT((worker->curState == worker->desiredState) &&
            (worker->curState == GET_STATE(RL_COMPUTE_OK, (RL_GET_PHASE_COUNT_DOWN(worker->pd, RL_COMPUTE_OK) - 1))));
    return NULL;
}

bool hcIsRunningWorker(ocrWorker_t * base) {
    // TODO: This states that we are in USER mode. Do we want to include RL_COMPUTE_OK?
    return (base->curState == GET_STATE(RL_USER_OK, 0));
}

/**
 * @brief Builds an instance of a HC worker
 */
ocrWorker_t* newWorkerHc(ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * worker = (ocrWorker_t*) runtimeChunkAlloc( sizeof(ocrWorkerHc_t), PERSISTENT_CHUNK);
    factory->initialize(factory, worker, perInstance);
    return worker;
}

/**
 * @brief Initialize an instance of a HC worker
 */
void initializeWorkerHc(ocrWorkerFactory_t * factory, ocrWorker_t* self, ocrParamList_t * perInstance) {
    initializeWorkerOcr(factory, self, perInstance);
    self->type = ((paramListWorkerHcInst_t*)perInstance)->workerType;
    u64 workerId = ((paramListWorkerHcInst_t*)perInstance)->workerId;;
    ASSERT((workerId && self->type == SLAVE_WORKERTYPE) ||
           (workerId == 0 && self->type == MASTER_WORKERTYPE));
    ocrWorkerHc_t * workerHc = (ocrWorkerHc_t*) self;
    workerHc->id = workerId;
    workerHc->hcType = HC_WORKER_COMP;
}

/******************************************************/
/* OCR-HC WORKER FACTORY                              */
/******************************************************/

void destructWorkerFactoryHc(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryHc(ocrParamList_t * perType) {
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryHc_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newWorkerHc;
    base->initialize = &initializeWorkerHc;
    base->destruct = &destructWorkerFactoryHc;

    base->workerFcts.destruct = FUNC_ADDR(void (*) (ocrWorker_t *), destructWorkerHc);
    base->workerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrWorker_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                       phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), hcWorkerSwitchRunlevel);
    base->workerFcts.run = FUNC_ADDR(void* (*) (ocrWorker_t *), hcRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*) (ocrWorker_t *), hcWorkShift);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*) (ocrWorker_t *), hcIsRunningWorker);
    return base;
}

#endif /* ENABLE_WORKER_HC */
