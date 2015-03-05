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
                // REC: TODO: Do we need a message to execute this
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
                // TODO: In the future, potentially take more than one)
                ASSERT(0);
            } else {
                // count = 0; no work received; do something else if required.
            }
        }
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
    workerXe->id = ((paramListWorkerXeInst_t*)perInstance)->workerId;
    workerXe->running = false;
}

void xeBeginWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {
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

void xeStartWorker(ocrWorker_t * base, ocrPolicyDomain_t * policy) {
    // Get a GUID
    guidify(policy, (u64)base, &(base->fguid), OCR_GUID_WORKER);
    base->pd = policy;
    base->location = policy->myLocation;

    ocrWorkerXe_t * xeWorker = (ocrWorkerXe_t *) base;
    xeWorker->running = true;

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

void* xeRunWorker(ocrWorker_t * worker) {
    // Need to pass down a data-structure
    ocrPolicyDomain_t *pd = worker->pd;

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

void xeStopWorker(ocrWorker_t * base, ocrRunLevel_t newRl) {
    switch(newRl) {
        case RL_STOP: {
            ocrWorkerXe_t * xeWorker = (ocrWorkerXe_t *) base;
            xeWorker->running = false;

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
    base->workerFcts.begin = FUNC_ADDR(void (*)(ocrWorker_t*, ocrPolicyDomain_t*), xeBeginWorker);
    base->workerFcts.start = FUNC_ADDR(void (*)(ocrWorker_t*, ocrPolicyDomain_t*), xeStartWorker);
    base->workerFcts.run = FUNC_ADDR(void* (*)(ocrWorker_t*), xeRunWorker);
    base->workerFcts.workShift = FUNC_ADDR(void* (*)(ocrWorker_t*), xeWorkShift);
    base->workerFcts.stop = FUNC_ADDR(void (*)(ocrWorker_t*,ocrRunLevel_t,u32), xeStopWorker);
    base->workerFcts.isRunning = FUNC_ADDR(bool (*)(ocrWorker_t*), xeIsRunningWorker);
    base->workerFcts.printLocation = FUNC_ADDR(void (*)(ocrWorker_t*, char* location), xePrintLocation);
    return base;
}

#endif /* ENABLE_WORKER_XE */
