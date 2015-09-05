/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_WORKER_XE

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"
#include "ocr-worker.h"
#include "ocr-db.h"
#include "worker/xe/xe-worker.h"

#ifdef HAL_FSIM_XE
#include "rmd-map.h"
#endif

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#include "policy-domain/xe/xe-policy.h"

#define DEBUG_TYPE WORKER

/******************************************************/
/* OCR-XE WORKER                                      */
/******************************************************/

// Convenient to have an id to index workers in pools
static inline u64 getWorkerId(ocrWorker_t * worker) {
    ocrWorkerXe_t * xeWorker = (ocrWorkerXe_t *) worker;
    return xeWorker->id;
}

/**
 * The computation worker routine that asks work to the scheduler
 */
static void workerLoop(ocrWorker_t * worker) {
    ocrPolicyDomain_t *pd = worker->pd;
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);
    while(worker->fcts.isRunning(worker)) {
    DPRINTF(DEBUG_LVL_VVERB, "XE %lx REQUESTING WORK\n", pd->myLocation);
#if 1 //This is disabled until we move TAKE heuristic in CE policy domain to inside scheduler
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_SCHED_GET_WORK
        msg.type = PD_MSG_SCHED_GET_WORK | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(schedArgs).kind = OCR_SCHED_WORK_EDT_USER;
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.guid = NULL_GUID;
        PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt.metaDataPtr = NULL;
        PD_MSG_FIELD_I(properties) = 0;
        if(pd->fcts.processMessage(pd, &msg, true) == 0) {
            // We got a response
            ocrFatGuid_t taskGuid = PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_WORK_EDT_USER).edt;
            if(taskGuid.guid != NULL_GUID) {
                DPRINTF(DEBUG_LVL_VVERB, "XE %lx EXECUTING TASK %lx\n", pd->myLocation, taskGuid.guid);
                // Task sanity checks
                ASSERT(taskGuid.metaDataPtr != NULL);
                worker->curTask = (ocrTask_t*)taskGuid.metaDataPtr;
                u32 factoryId = PD_MSG_FIELD_O(factoryId);
                pd->taskFactories[factoryId]->fcts.execute(worker->curTask);
#undef PD_TYPE
#define PD_TYPE PD_MSG_SCHED_NOTIFY
                getCurrentEnv(NULL, NULL, NULL, &msg);
                msg.type = PD_MSG_SCHED_NOTIFY | PD_MSG_REQUEST;
                PD_MSG_FIELD_IO(schedArgs).kind = OCR_SCHED_NOTIFY_EDT_DONE;
                PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_DONE).guid.guid = taskGuid.guid;
                PD_MSG_FIELD_IO(schedArgs).OCR_SCHED_ARG_FIELD(OCR_SCHED_NOTIFY_EDT_DONE).guid.metaDataPtr = taskGuid.metaDataPtr;
                ASSERT(pd->fcts.processMessage(pd, &msg, false) == 0);

                // Important for this to be the last
                worker->curTask = NULL;
            } else {
                DPRINTF(DEBUG_LVL_VVERB, "XE %lx NULL RESPONSE from CE\n", pd->myLocation);
            }
        }
#undef PD_MSG
#undef PD_TYPE

#else
        ocrFatGuid_t taskGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
        u32 count = 1;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_COMM_TAKE
        msg.type = PD_MSG_COMM_TAKE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guids) = &taskGuid;
        PD_MSG_FIELD_IO(guidCount) = count;
        PD_MSG_FIELD_I(properties) = 0;
        PD_MSG_FIELD_IO(type) = OCR_GUID_EDT;
        if(pd->fcts.processMessage(pd, &msg, true) == 0) {
            // We got a response
            count = PD_MSG_FIELD_IO(guidCount);
            if(count == 1) {
                taskGuid = PD_MSG_FIELD_IO(guids[0]);
                ASSERT(taskGuid.guid != NULL_GUID && taskGuid.metaDataPtr != NULL);
                worker->curTask = (ocrTask_t*)taskGuid.metaDataPtr;
                u8 (*executeFunc)(ocrTask_t *) = (u8 (*)(ocrTask_t*))PD_MSG_FIELD_IO(extra); // Execute is stored in extra
                executeFunc(worker->curTask);
                worker->curTask = NULL;
#undef PD_TYPE
                // Destroy the work
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
            } else if (count > 1) {
                // BUG #586: GIVE/TAKE will go away and multiple work items may be transferred
                ASSERT(0);
            } else {
                // count = 0; no work received; do something else if required.
            }
        }
#endif
    } /* End of while loop */
}

void destructWorkerXe(ocrWorker_t * base) {
    u64 i = 0;
    while(i < base->computeCount) {
        base->computes[i]->fcts.destruct(base->computes[i]);
        ++i;
    }
    runtimeChunkFree((u64)(base->computes), NULL);
    runtimeChunkFree((u64)base, NULL);
}


u8 xeWorkerSwitchRunlevel(ocrWorker_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
            ((ocrWorkerXe_t *) self)->running = true;
            DPRINTF(DEBUG_LVL_INFO, "XE %lx Started\n", self->location);
            break;
        case RL_USER_OK:
            ((ocrWorkerXe_t *) self)->running = false;
            DPRINTF(DEBUG_LVL_INFO, "XE %lx Stopped\n", self->location);
            break;
        default:
            break;
    }

    toReturn |= self->computes[0]->fcts.switchRunlevel(self->computes[0], PD, runlevel, phase, properties,
                                                           callback, val);

    return toReturn;
}

/**
 * Builds an instance of a XE worker
 */
ocrWorker_t* newWorkerXe (ocrWorkerFactory_t * factory, ocrParamList_t * perInstance) {
    ocrWorker_t * base = (ocrWorker_t*)runtimeChunkAlloc(sizeof(ocrWorkerXe_t), PERSISTENT_CHUNK);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeWorkerXe(ocrWorkerFactory_t * factory, ocrWorker_t* base, ocrParamList_t * perInstance) {
    initializeWorkerOcr(factory, base, perInstance);
    base->type = SLAVE_WORKERTYPE;


    ocrWorkerXe_t* workerXe = (ocrWorkerXe_t*) base;
    workerXe->id = ((paramListWorkerInst_t*)perInstance)->workerId;
    workerXe->running = false;
}

void* xeRunWorker(ocrWorker_t * worker) {
    // Need to pass down a data-structure
    ocrPolicyDomain_t *pd = worker->pd;

    u32 i;
    for(i = 0; i < worker->computeCount; i++)
        worker->computes[i]->fcts.setCurrentEnv(worker->computes[i], pd, worker);

    if (pd->myLocation == 0) { //Blessed worker

        // This is all part of the mainEdt setup
        // and should be executed by the "blessed" worker.

        void * packedUserArgv;
#if defined(SAL_FSIM_XE)
        packedUserArgv = ((ocrPolicyDomainXe_t *) pd)->packedArgsLocation;
        extern ocrGuid_t mainEdt( u32, u64 *, u32, ocrEdtDep_t * );
#else
        packedUserArgv = userArgsGet();
        ocrEdt_t mainEdt = mainEdtGet();
#endif

        u64 totalLength = ((u64*) packedUserArgv)[0]; // already exclude this first arg
        // strip off the 'totalLength first argument'
        packedUserArgv = (void *) (((u64)packedUserArgv) + sizeof(u64)); // skip first totalLength argument
        ocrGuid_t dbGuid;
        void* dbPtr;
        ocrDbCreate(&dbGuid, &dbPtr, totalLength,
                    DB_PROP_IGNORE_WARN, NULL_GUID, NO_ALLOC);

        // copy packed args to DB
        hal_memCopy(dbPtr, packedUserArgv, totalLength, 0);
        // Release the DB so that mainEdt can acquire it.
        // Do not invoke ocrDbRelease to avoid the warning there.
        PD_MSG_STACK(msg);
        getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_RELEASE
        msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid.guid) = dbGuid;
        PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(edt.guid) = NULL_GUID;
        PD_MSG_FIELD_I(edt.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(ptr) = NULL;
        PD_MSG_FIELD_I(size) = 0;
        PD_MSG_FIELD_I(properties) = 0;
        RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
#undef PD_MSG
#undef PD_TYPE

        // Create mainEDT and then allow it to be scheduled
        // This gives mainEDT a GUID which is useful for some book-keeping business
        ocrGuid_t edtTemplateGuid = NULL_GUID, edtGuid = NULL_GUID;
        ocrEdtTemplateCreate(&edtTemplateGuid, mainEdt, 0, 1);
        ocrEdtCreate(&edtGuid, edtTemplateGuid, EDT_PARAM_DEF, /* paramv=*/ NULL,
                     EDT_PARAM_DEF, /* depv=*/&dbGuid, EDT_PROP_NONE, NULL_GUID, NULL);
    }

    DPRINTF(DEBUG_LVL_INFO, "Starting scheduler routine of worker %ld\n", getWorkerId(worker));
    workerLoop(worker);
    return NULL;
}

void* xeWorkShift(ocrWorker_t* worker) {
    ASSERT(0); // Not supported
    return NULL;
}

bool xeIsRunningWorker(ocrWorker_t * base) {
    ocrWorkerXe_t * xeWorker = (ocrWorkerXe_t *) base;
    return xeWorker->running;
}

void xePrintLocation(ocrWorker_t *base, char* location) {
#ifdef HAL_FSIM_XE
    SNPRINTF(location, 32, "XE %d Block %d Unit %d", AGENT_FROM_ID(base->location),
             BLOCK_FROM_ID(base->location), UNIT_FROM_ID(base->location));
#else
    SNPRINTF(location, 32, "XE");
#endif
}

/******************************************************/
/* OCR-XE WORKER FACTORY                              */
/******************************************************/

void destructWorkerFactoryXe(ocrWorkerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrWorkerFactory_t * newOcrWorkerFactoryXe(ocrParamList_t * perType) {
    ocrWorkerFactory_t* base = (ocrWorkerFactory_t*)runtimeChunkAlloc(sizeof(ocrWorkerFactoryXe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newWorkerXe;
    base->initialize = &initializeWorkerXe;
    base->destruct = &destructWorkerFactoryXe;

    base->workerFcts.destruct = FUNC_ADDR(void (*)(ocrWorker_t*), destructWorkerXe);
    base->workerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrWorker_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                       phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), xeWorkerSwitchRunlevel);
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), xeRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*)(ocrWorker_t*), xeWorkShift);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*)(ocrWorker_t*), xeIsRunningWorker);
    base->workerFcts.printLocation = FUNC_ADDR(void (*)(ocrWorker_t*, char* location), xePrintLocation);
    return base;
}

#endif /* ENABLE_WORKER_XE */
