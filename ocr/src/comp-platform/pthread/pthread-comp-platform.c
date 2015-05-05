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
    // This should be superfluous (done in hc-worker in hcRunWorker
    /*
    data->pd = pthreadCompPlatform->base.pd;
    data->worker = pthreadCompPlatform->base.worker;
    */
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


u8 pthreadSwitchRunlevel(ocrCompPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                         phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // The worker is the capable module and we operate as
    // inert wrt it
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    ocrCompPlatformPthread_t *pthreadCompPlatform = (ocrCompPlatformPthread_t*)self;
    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
            ASSERT(self->worker != NULL);
        }
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            self->pd = PD;
            if(properties & RL_PD_MASTER) {
                // We need to make sure we have the current environment set
                // at least partially as the PD may be used
                self->fcts.setCurrentEnv(self, self->pd, NULL);
            }
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
            if(properties & RL_PD_MASTER) {
                // We do not need to create another thread
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
            } else {
                // We need to create another capable module
                ocrCompPlatformPthread_t * pthreadCompPlatform = (ocrCompPlatformPthread_t *)self;
                pthread_attr_t attr;
                toReturn |= pthread_attr_init(&attr);
                //Note this call may fail if the system doesn't like the stack size asked for.
                if(!toReturn)
                    toReturn |= pthread_attr_setstacksize(&attr, pthreadCompPlatform->stackSize);
                if(!toReturn) {
                    toReturn |= pthread_create(&(pthreadCompPlatform->osThread),
                                               &attr, &pthreadRoutineWrapper,
                                               pthreadCompPlatform);
                }
            }
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
            // At this point, this is run only by the master thread
            if(!(properties & RL_PD_MASTER)) {
                // We do not join with ourself
                toReturn |= pthread_join(pthreadCompPlatform->osThread, NULL);
            }
        }
        break;
    case RL_USER_OK:
        break;
    default:
        ASSERT(0);
    }
    return toReturn;
}

#if 0
    // Not sure if I need to do this anymore for master thread
    if(pthreadCompPlatform->isMaster) {
        // Destroy keys for the master thread
        void* _t = pthread_getspecific(selfKey);
        destroyKey(_t);
#ifdef OCR_RUNTIME_PROFILER
        _t = pthread_getspecific(_profilerThreadData);
        _profilerDataDestroy(_t);
#endif
    }
#endif

u8 pthreadGetThrottle(ocrCompPlatform_t *self, u64* value) {
    return 1;
}

u8 pthreadSetThrottle(ocrCompPlatform_t *self, u64 value) {
    return 1;
}

u8 pthreadSetCurrentEnv(ocrCompPlatform_t *self, ocrPolicyDomain_t *pd,
                        ocrWorker_t *worker) {

    ASSERT(pd->fguid.guid == self->pd->fguid.guid);
    perThreadStorage_t *vals = pthread_getspecific(selfKey);
    vals->pd = pd;
    vals->worker = worker;
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
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), pthreadSwitchRunlevel);
    base->platformFcts.getThrottle = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, u64*), pthreadGetThrottle);
    base->platformFcts.setThrottle = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, u64), pthreadSetThrottle);
    base->platformFcts.setCurrentEnv = FUNC_ADDR(u8 (*)(ocrCompPlatform_t*, ocrPolicyDomain_t*, ocrWorker_t*), pthreadSetCurrentEnv);

    paramListCompPlatformPthread_t * params =
        (paramListCompPlatformPthread_t *) perType;
    derived->stackSize = ((params != NULL) && (params->stackSize > 0)) ? params->stackSize : 8388608;

    return base;
}
#endif /* ENABLE_COMP_PLATFORM_PTHREAD */

