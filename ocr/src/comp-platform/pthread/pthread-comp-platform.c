/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMP_PLATFORM_PTHREAD

#include "debug.h"

#include "ocr-policy-domain.h"

#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"
#include "ocr-worker.h"

#include "pthread-comp-platform.h"

#include <stdlib.h> // For malloc for TLS

#include "utils/profiler/profiler.h"

#define DEBUG_TYPE COMP_PLATFORM

extern void bindThread(u32 mask);

/**
 * @brief Structure stored on a per-thread basis to keep track of
 * "who we are"
 */
typedef struct {
    ocrPolicyDomain_t *pd;
    ocrWorker_t *worker;
} perThreadStorage_t;

/**
 * The key we use to be able to find which compPlatform we are on
 */
pthread_key_t selfKey;

/**
 * The keyInit boolean allows threads to know if they can lookup the TLS key or not.
 * TODO: may disappear once we clearly define runlevels trac #80
 */
static bool keyInit = false;

#ifdef OCR_RUNTIME_PROFILER
pthread_key_t _profilerThreadData;
#endif

pthread_once_t selfKeyInitialized = PTHREAD_ONCE_INIT;

static void * pthreadRoutineExecute(ocrWorker_t * worker) {
    return worker->fcts.run(worker);
}

static void * pthreadRoutineWrapper(void * arg) {
    // Only called on slave workers (never master)
    ocrCompPlatformPthread_t * pthreadCompPlatform = (ocrCompPlatformPthread_t *) arg;
    s32 cpuBind = pthreadCompPlatform->binding;
    if(cpuBind != -1) {
        DPRINTF(DEBUG_LVL_INFO, "Binding comp-platform to cpu_id %d\n", cpuBind);
        bindThread(cpuBind);
    }
    // Wrapper routine to allow initialization of local storage
    // before entering the worker routine.
    perThreadStorage_t *data = (perThreadStorage_t*)malloc(sizeof(perThreadStorage_t));
    data->pd = pthreadCompPlatform->base.pd;
    data->worker = pthreadCompPlatform->base.worker;
    RESULT_ASSERT(pthread_setspecific(selfKey, data), ==, 0);

#ifdef OCR_RUNTIME_PROFILER
    _profilerData *d = (_profilerData*)malloc(sizeof(_profilerData));
    char buffer[50];
    snprintf(buffer, 50, "profiler_%lx-%lx", data->pd->myLocation, (u64)arg);
    d->output = fopen(buffer, "w");
    ASSERT(d->output);
    RESULT_ASSERT(pthread_setspecific(_profilerThreadData, d), ==, 0);
#endif

    return pthreadRoutineExecute(pthreadCompPlatform->base.worker);
}

static void destroyKey(void* arg) {
    runtimeChunkFree((u64)arg, NULL);
}

/**
 * Called once by the master thread before others pthread are started.
 */
static void initializeKey() {
    RESULT_ASSERT(pthread_key_create(&selfKey, &destroyKey), ==, 0);
#ifdef OCR_RUNTIME_PROFILER
    RESULT_ASSERT(pthread_key_create(&_profilerThreadData, &_profilerDataDestroy), ==, 0);
#endif

    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    // We are going to set our own key (we are the master thread)
    perThreadStorage_t *data = (perThreadStorage_t*)malloc(sizeof(perThreadStorage_t));
    data->pd = NULL;
    data->worker = NULL;
    RESULT_ASSERT(pthread_setspecific(selfKey, data), ==, 0);

    keyInit = true;
}

void pthreadDestruct (ocrCompPlatform_t * base) {
    // runtimeChunkFree((u64)base, NULL);
}

void pthreadBegin(ocrCompPlatform_t * compPlatform, ocrPolicyDomain_t * PD, ocrWorkerType_t workerType) {

    ocrCompPlatformPthread_t * pthreadCompPlatform = (ocrCompPlatformPthread_t *) compPlatform;
    compPlatform->pd = PD;
    pthreadCompPlatform->isMaster = (workerType == MASTER_WORKERTYPE);

    if(pthreadCompPlatform->isMaster) {
        // Only do the binding
        s32 cpuBind = pthreadCompPlatform->binding;
        if(cpuBind != -1) {
            DPRINTF(DEBUG_LVL_INFO, "Binding comp-platform to cpu_id %d\n", cpuBind);
            bindThread(cpuBind);
        }
#ifdef OCR_RUNTIME_PROFILER
        {
            _profilerData *d = (_profilerData*)malloc(sizeof(_profilerData));
            char buffer[50];
            snprintf(buffer, 50, "profiler_%lx-%lx", PD->myLocation, 0UL);
            d->output = fopen(buffer, "w");
            ASSERT(d->output);
            RESULT_ASSERT(pthread_setspecific(_profilerThreadData, d), ==, 0);
        }
#endif
        // The master starts executing when we call "stop" on it
    }
}

void pthreadStart(ocrCompPlatform_t * compPlatform, ocrPolicyDomain_t * PD, ocrWorker_t * worker) {
    ocrCompPlatformPthread_t * pthreadCompPlatform = (ocrCompPlatformPthread_t *) compPlatform;
    compPlatform->worker = worker;

    if(!pthreadCompPlatform->isMaster) {
        pthread_attr_t attr;
        RESULT_ASSERT(pthread_attr_init(&attr), ==, 0);
        //Note this call may fail if the system doesn't like the stack size asked for.
        RESULT_ASSERT(pthread_attr_setstacksize(&attr, pthreadCompPlatform->stackSize), ==, 0);
        RESULT_ASSERT(pthread_create(&(pthreadCompPlatform->osThread),
                                     &attr, &pthreadRoutineWrapper,
                                     pthreadCompPlatform), ==, 0);
    } else {
        // The upper level worker will only start us once
        pthreadRoutineExecute(worker);
    }
}

void pthreadStop(ocrCompPlatform_t * compPlatform, ocrRunLevel_t newRl, u32 action) {
    switch(newRl) {
        case RL_STOP: {
            // A call to 'stop' should be the last call of the 'pthreadRoutineWrapper'
            // function given at the thread's creation time.

            // Note: All the code accessing this thread's TLS (freeing, clean-up, etc.)
            // must be added to the 'destroyKey' function. It is to be called when each
            // of the threads (except the master thread) are joined. The master thread's
            // clean up can be done in the 'pthreadFinish' function (it calls destroyKey).
            break;
        }
        case RL_SHUTDOWN: {
            //TODO-RL, it would be nice to move this code in stop, so that we can guarantee everybody
            // but the master thread is running
            // This code will be called by the main thread to join everyone else
            ocrCompPlatformPthread_t *pthreadCompPlatform = (ocrCompPlatformPthread_t*)compPlatform;
            if(!pthreadCompPlatform->isMaster) {
                RESULT_ASSERT(pthread_join(pthreadCompPlatform->osThread, NULL), ==, 0);
            } else {
                // master thread, do nothing
            }
            break;
        }
        case RL_DEALLOCATE: {
            ocrCompPlatformPthread_t *pthreadCompPlatform = (ocrCompPlatformPthread_t*)compPlatform;
            if(pthreadCompPlatform->isMaster) {
                // Destroy keys for the master thread
                void* _t = pthread_getspecific(selfKey);
                destroyKey(_t);
            #ifdef OCR_RUNTIME_PROFILER
                _t = pthread_getspecific(_profilerThreadData);
                _profilerDataDestroy(_t);
            #endif
            }
            runtimeChunkFree((u64)compPlatform, NULL);
            break;
        }
        default:
            ASSERT("Unknown runlevel in stop function");
    }
}

u8 pthreadGetThrottle(ocrCompPlatform_t *self, u64* value) {
    return 1;
}

u8 pthreadSetThrottle(ocrCompPlatform_t *self, u64 value) {
    return 1;
}

u8 pthreadSetCurrentEnv(ocrCompPlatform_t *self, ocrPolicyDomain_t *pd,
                        ocrWorker_t *worker) {

    ASSERT(pd->fguid.guid == self->pd->fguid.guid);
    ocrCompPlatformPthread_t *pthreadCompPlatform = (ocrCompPlatformPthread_t*)self;
    if(pthreadCompPlatform->isMaster) {
        perThreadStorage_t *vals = pthread_getspecific(selfKey);
        vals->pd = pd;
        vals->worker = worker;
    }
    return 0;
}

ocrCompPlatform_t* newCompPlatformPthread(ocrCompPlatformFactory_t *factory,
        ocrParamList_t *perInstance) {
    // initializeKey is called once and it's always the master thread since
    // it is the thread bringing up the runtime.
    pthread_once(&selfKeyInitialized,  initializeKey);
    ocrCompPlatformPthread_t * compPlatformPthread = (ocrCompPlatformPthread_t*)
            runtimeChunkAlloc(sizeof(ocrCompPlatformPthread_t), PERSISTENT_CHUNK);

    ocrCompPlatform_t * derived = (ocrCompPlatform_t *) compPlatformPthread;
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeCompPlatformPthread(ocrCompPlatformFactory_t * factory, ocrCompPlatform_t * derived, ocrParamList_t * perInstance) {
    initializeCompPlatformOcr(factory, derived, perInstance);
    paramListCompPlatformPthread_t * params =
        (paramListCompPlatformPthread_t *) perInstance;

    ocrCompPlatformPthread_t *compPlatformPthread = (ocrCompPlatformPthread_t *)derived;
    compPlatformPthread->base.fcts = factory->platformFcts;
    compPlatformPthread->binding = (params != NULL) ? params->binding : -1;
    compPlatformPthread->stackSize = ((params != NULL) && (params->stackSize > 0)) ? params->stackSize : 8388608;
    compPlatformPthread->isMaster = false;
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCompPlatformFactoryPthread(ocrCompPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void getCurrentEnv(ocrPolicyDomain_t** pd, ocrWorker_t** worker,
                   ocrTask_t **task, ocrPolicyMsg_t* msg) {
    START_PROFILE(cp_getCurrentEnv);
    if(keyInit) {
        perThreadStorage_t *vals = pthread_getspecific(selfKey);
        if(vals == NULL)
            return;
        if(pd)
            *pd = vals->pd;
        if(worker)
            *worker = vals->worker;
        if(vals->worker && task && ((vals->worker->curTask != ((ocrTask_t*)0x1))))
            //TODO the 0x1 is going away with the fix for runlevel trac #80)
            *task = vals->worker->curTask;
        if(msg) {
            //By default set src and dest location to current location.
            msg->srcLocation = vals->pd->myLocation;
            msg->destLocation = msg->srcLocation;
            //msg->seqId = vals->worker->seqId;
            msg->usefulSize = 0; // Convention to say that the size is not yet set
        }
    }
    RETURN_PROFILE();
}

ocrCompPlatformFactory_t *newCompPlatformFactoryPthread(ocrParamList_t *perType) {
    ocrCompPlatformFactory_t *base = (ocrCompPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCompPlatformFactoryPthread_t), NONPERSISTENT_CHUNK);

    ocrCompPlatformFactoryPthread_t * derived = (ocrCompPlatformFactoryPthread_t *) base;

    base->instantiate = &newCompPlatformPthread;
    base->initialize = &initializeCompPlatformPthread;
    base->destruct = &destructCompPlatformFactoryPthread;
    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCompPlatform_t*), pthreadDestruct);
    base->platformFcts.begin = FUNC_ADDR(void (*)(ocrCompPlatform_t*, ocrPolicyDomain_t*, ocrWorkerType_t), pthreadBegin);
    base->platformFcts.start = FUNC_ADDR(void (*)(ocrCompPlatform_t*, ocrPolicyDomain_t*, ocrWorker_t*), pthreadStart);
    base->platformFcts.stop = FUNC_ADDR(void (*)(ocrCompPlatform_t*,ocrRunLevel_t,u32), pthreadStop);
    base->platformFcts.getThrottle = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, u64*), pthreadGetThrottle);
    base->platformFcts.setThrottle = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, u64), pthreadSetThrottle);
    base->platformFcts.setCurrentEnv = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, ocrPolicyDomain_t*, ocrWorker_t*), pthreadSetCurrentEnv);

    paramListCompPlatformPthread_t * params =
        (paramListCompPlatformPthread_t *) perType;
    derived->stackSize = ((params != NULL) && (params->stackSize > 0)) ? params->stackSize : 8388608;

    return base;
}
#endif /* ENABLE_COMP_PLATFORM_PTHREAD */

