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

static void hcNotifyRunlevelToPd(ocrWorker_t * worker, ocrRunLevel_t newRl, u32 action) {
    ocrPolicyDomain_t * pd;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MGT_RL_NOTIFY
    msg.type = PD_MSG_MGT_RL_NOTIFY | PD_MSG_REQUEST;
    //TODO-RL: for now notify is implicitely meaning the worker reached the runlevel
    PD_MSG_FIELD_I(runlevel) = newRl;
    PD_MSG_FIELD_I(action) = action;
    pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
}

static void runlevel_stop(ocrWorker_t * base) {
    //
    // ENTERING THE STOP RUNLEVEL
    //
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
    DPRINTF(DEBUG_LVL_INFO, "Stopping worker %ld\n", getWorkerId(base));
}

static void workerLoop(ocrWorker_t * worker) {
    // Runlevel USER 'computation' loop
    ocrWorkerHc_t * self = ((ocrWorkerHc_t *) worker);

    while(self->workLoopSpin) {
        START_PROFILE(wo_hc_workerLoop);
        worker->fcts.workShift(worker);
        EXIT_PROFILE;
    }
    // Received a RL_ACTION_QUIESCE_COMP that broke the loop
    ASSERT(worker->rl == RL_RUNNING_USER_WIP);

    // Notify the policy-domain we have quiesced
    // PRINTF("[%d] WORKER[%d] RL_ACTION_QUIESCE_COMP\n", (int)worker->pd->myLocation, ((ocrWorkerHc_t*) worker)->id);
    self->workLoopSpin = true;
    hcNotifyRunlevelToPd(worker, RL_RUNNING_USER, RL_ACTION_QUIESCE_COMP);

    // While the worker transitions it keep accepting work.
    // NOTE: the rationale for doing this is that other workers are
    // potentially wrapping up their user EDTs and that might create
    // runtime EDTs depending on the implementation. Hence, keep
    // working until the PD says so.

    // Runlevel USER_WIP 'keep working' loop
    while(self->workLoopSpin) {
        START_PROFILE(wo_hc_workerLoop);
        worker->fcts.workShift(worker);
        EXIT_PROFILE;
    }
    // Received a RL_ACTION_EXIT that broke the loop

    // Example of a runlevel 'barrier': The PD is responsible to
    // transition the runlevel of all workers whenever it sees fit.
    // This barrier ensures that all other workers have exited their
    // RL_RUNNING_USER before proceeding to the next runlevel.
    hcNotifyRunlevelToPd(worker, RL_RUNNING_USER, RL_ACTION_EXIT);
    while(worker->rl == RL_RUNNING_USER_WIP);
    ASSERT(worker->rl == RL_RUNNING_RT);

    // Nothing to do in RT mode for workers
    worker->rl = RL_RUNNING_RT_WIP;
    hcNotifyRunlevelToPd(worker, RL_RUNNING_RT, RL_ACTION_EXIT);

    while(worker->rl == RL_RUNNING_RT_WIP);
    // Enter the stop runlevel
    ASSERT(worker->rl == RL_STOP);
    runlevel_stop(worker);

    worker->rl = RL_STOP_WIP;
    hcNotifyRunlevelToPd(worker, RL_STOP, RL_ACTION_EXIT);

    //
    // This instance of worker becomes inert
    //
}

void destructWorkerHc(ocrWorker_t * base) {
    // u64 i = 0;
    // while(i < base->computeCount) {
    //     //base->computes[i]->fcts.destruct(base->computes[i]);
    //     base->computes[i]->fcts.stop(base->computes[i], RL_DEALLOCATE, RL_ACTION_ENTER);
    //     ++i;
    // }
    // runtimeChunkFree((u64)(base->computes), NULL);
    // runtimeChunkFree((u64)base, NULL);
}

void hcBeginWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {
    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;
    base->location = (ocrLocation_t)base;
    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; ++i) {
        base->computes[i]->fcts.begin(base->computes[i], policy, base->type);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
    }

    if(base->type == MASTER_WORKERTYPE) {
        // For the master thread, we need to set the PD and worker
        // The other threads will set this when they start
        for(i = 0; i < computeCount; ++i) {
            base->computes[i]->fcts.setCurrentEnv(base->computes[i], policy, base);
        }
    }
}

void hcStartWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {

    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;

    if(!hcWorker->secondStart) {
        // Get a GUID
        guidify(policy, (u64)base, &(base->fguid), OCR_GUID_WORKER);
        base->pd = policy;
        if(base->type == MASTER_WORKERTYPE) {
            hcWorker->secondStart = true; // Only relevant for MASTER_WORKERTYPE
            return; // Don't start right away
        }
    }

    ASSERT(base->type != MASTER_WORKERTYPE || hcWorker->secondStart);
    // u32 oldValue = hal_cmpswap32(&base->rl, RL_DEFAULT, RL_RUNNING);
    // ASSERT(oldValue == RL_DEFAULT);
    ASSERT(base->rl == RL_DEFAULT);
    base->rl = RL_RUNNING_USER;

    // Starts everybody, the first comp-platform has specific
    // code to represent the master thread.
    u64 computeCount = base->computeCount;
    // What the compute target will execute
    ASSERT(computeCount == 1);
    u64 i = 0;
    for(i = 0; i < computeCount; ++i) {
        base->computes[i]->fcts.start(base->computes[i], policy, base);
#ifdef OCR_ENABLE_STATISTICS
        statsWORKER_START(policy, base->guid, base, base->computes[i]->guid, base->computes[i]);
#endif
    }
    // Otherwise, it is highly likely that we are shutting down
}

static bool isMainEdtForker(ocrWorker_t * worker, ocrGuid_t * affinityMasterPD) {
    // Determine if current worker is the master worker of this PD
    bool blessedWorker = (worker->type == MASTER_WORKERTYPE);
    // When OCR is used in library mode, there's no mainEdt
    blessedWorker &= (mainEdtGet() != NULL);
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

void* hcRunWorker(ocrWorker_t * worker) {
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

    ocrGuid_t affinityMasterPD;
    bool forkMain = isMainEdtForker(worker, &affinityMasterPD);
    if (forkMain) {
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
    } else {
        // Set who we are
        ocrPolicyDomain_t *pd = worker->pd;
        u32 i;
        for(i = 0; i < worker->computeCount; ++i) {
            worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);
        }
    }

    DPRINTF(DEBUG_LVL_INFO, "Starting scheduler routine of worker %ld\n", getWorkerId(worker));
    workerLoop(worker);
    return NULL;
}

void hcStopWorker(ocrWorker_t * base, ocrRunLevel_t rl, u32 actionRl) {
    if (actionRl == RL_ACTION_ENTER) {
        switch(rl) {
            case RL_RUNNING_RT: {
                ASSERT(base->rl == RL_RUNNING_USER_WIP);
                base->rl = RL_RUNNING_RT;
                break;
            }
            case RL_STOP: {
                ASSERT(base->rl == RL_RUNNING_RT_WIP);
                base->rl = RL_STOP;
                break;
            }
            case RL_SHUTDOWN: {
                ASSERT(base->rl == RL_STOP_WIP);
                base->rl = RL_SHUTDOWN;
                DPRINTF(DEBUG_LVL_INFO, "Finishing worker routine %ld\n", getWorkerId(base));
                ASSERT(base->computeCount == 1);
                u64 i = 0;
                for(i = 0; i < base->computeCount; i++) {
                    base->computes[i]->fcts.stop(base->computes[i], RL_SHUTDOWN, actionRl);
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
                //TODO do this in another runlevel, for now we're just shutting down
                #define PD_MSG (&msg)
                #define PD_TYPE PD_MSG_GUID_DESTROY
                    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                    PD_MSG_FIELD_I(guid) = base->fguid;
                    PD_MSG_FIELD_I(properties) = 0;
                    // Ignore failure here, we are most likely shutting down
                    base->pd->fcts.processMessage(base->pd, &msg, false);
                #undef PD_MSG
                #undef PD_TYPE
                    base->fguid.guid = UNINITIALIZED_GUID;

                base->rl = RL_SHUTDOWN_WIP;
                hcNotifyRunlevelToPd(base, RL_SHUTDOWN, RL_ACTION_EXIT);
                break;
            }
            case RL_DEALLOCATE: {
                u64 i = 0;
                while(i < base->computeCount) {
                    base->computes[i]->fcts.stop(base->computes[i], RL_DEALLOCATE, RL_ACTION_ENTER);
                    ++i;
                }
                runtimeChunkFree((u64)(base->computes), NULL);
                runtimeChunkFree((u64)base, NULL);
                break;
            }
            default:
                ASSERT("No implementation to enter runlevel");
        }
    } else {
        switch(rl) {
            case RL_RUNNING_USER: {
                ASSERT((base->rl == RL_RUNNING_USER) || (base->rl == RL_RUNNING_USER_WIP));
                if (actionRl == RL_ACTION_QUIESCE_COMP) {
                    // Make the worker exit its current work loop
                    base->rl = RL_RUNNING_USER_WIP;
                    ocrWorkerHc_t * self = (ocrWorkerHc_t *) base;
                    // Break the 'computation' loop
                    self->workLoopSpin = false;
                } else if (actionRl == RL_ACTION_EXIT) {
                    // Break the 'keep working' loop
                    ocrWorkerHc_t * self = (ocrWorkerHc_t *) base;
                    self->workLoopSpin = false;
                } else {
                    // nothing to do for other actions, just notify it went through
                    hcNotifyRunlevelToPd(base, RL_RUNNING_USER, actionRl);
                }
                break;
            }
            case RL_RUNNING_RT: {
                ASSERT((base->rl == RL_RUNNING_RT) || (base->rl == RL_RUNNING_RT_WIP));
                ASSERT(actionRl == RL_ACTION_EXIT);
                ocrWorkerHc_t * self = (ocrWorkerHc_t *) base;
                self->workLoopSpin = false;
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

bool hcIsRunningWorker(ocrWorker_t * base) {
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) base;
    //TODO-RL: define exactly what running means ?
    //Currently serving edts ? or worker rl being RL_USER/RL_RT ?
    return hcWorker->workLoopSpin;
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
    workerHc->secondStart = false;
    workerHc->hcType = HC_WORKER_COMP;
    workerHc->workLoopSpin = true;
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
    base->workerFcts.begin = FUNC_ADDR(void (*) (ocrWorker_t *, ocrPolicyDomain_t *), hcBeginWorker);
    base->workerFcts.start = FUNC_ADDR(void (*) (ocrWorker_t *, ocrPolicyDomain_t *), hcStartWorker);
    base->workerFcts.run = FUNC_ADDR(void* (*) (ocrWorker_t *), hcRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*) (ocrWorker_t *), hcWorkShift);
    base->workerFcts.stop = FUNC_ADDR(void (*) (ocrWorker_t *,ocrRunLevel_t,u32), hcStopWorker);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*) (ocrWorker_t *), hcIsRunningWorker);
    return base;
}

#endif /* ENABLE_WORKER_HC */
