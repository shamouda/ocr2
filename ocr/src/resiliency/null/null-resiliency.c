/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_RESILIENCY_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-resiliency.h"
#include "ocr-hal.h"

#include "policy-domain/hc/hc-policy.h"
#include "resiliency/null/null-resiliency.h"

#define DEBUG_TYPE RESILIENCY

#define WORKER_FREQ_NORMAL 2000
#define FREQUENCY_SLOWDOWN_THRESHOLD 10

/******************************************************/
/* OCR-NULL RESILIENCY                                */
/******************************************************/

ocrResiliency_t* newResiliencyNull(ocrResiliencyFactory_t * factory, ocrParamList_t *perInstance) {
    ocrResiliency_t* self = (ocrResiliency_t*) runtimeChunkAlloc(sizeof(ocrResiliencyNull_t), PERSISTENT_CHUNK);
    initializeResiliencyOcr(factory, self, perInstance);
    return self;
}

u8 nullResiliencySwitchRunlevel(ocrResiliency_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                        phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

void nullResiliencyDestruct(ocrResiliency_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 nullResiliencyInvoke(ocrResiliency_t *self, ocrPolicyMsg_t *msg, u32 properties) {
    if (properties == RM_INVOKE_PROP_PROACTIVE) {
        ocrPolicyDomain_t *pd;
        ocrWorker_t *worker;
        getCurrentEnv(&pd, &worker, NULL, NULL);
        if (hal_cmpswap32(&(pd->rmMaster), 0, 1) == 0) {
            DPRINTF(DEBUG_LVL_VERB, "Resiliency Manager Proactive Monitoring PD... (worker %lu)\n", (u64)worker->id);
            pd->allocators[0]->fcts.queryOp(pd->allocators[0], QUERY_ALLOC_MAXSIZE, 0, NULL);
            pd->allocators[0]->fcts.queryOp(pd->allocators[0], QUERY_ALLOC_FREEBYTES, 0, NULL);
            ASSERT(hal_cmpswap32(&(pd->rmMaster), 1, 0) == 1);
        }
        DPRINTF(DEBUG_LVL_VVERB, "Resiliency Manager Proactive Monitoring Platform... (worker %lu)\n", (u64)worker->id);
        ocrCompTarget_t *compTarget = pd->workers[0]->computes[0];
        compTarget->fcts.queryOp(compTarget, QUERY_COMP_CUR_TEMP, 0, NULL);
        compTarget->fcts.queryOp(compTarget, QUERY_COMP_CUR_FREQ, 0, NULL);
        if (worker->curFreq < WORKER_FREQ_NORMAL) {
            u32 changePercentage = ((WORKER_FREQ_NORMAL - worker->curFreq) * 100) / WORKER_FREQ_NORMAL;
            if (changePercentage > FREQUENCY_SLOWDOWN_THRESHOLD) {
                if (worker->active) {
                    DPRINTF(DEBUG_LVL_WARN, "%u%% frequency slowdown detected on worker %lu. Above threshold (%u%%). Deactivating worker!\n", changePercentage, (u64)worker->id, FREQUENCY_SLOWDOWN_THRESHOLD);
                    worker->active = false;
                }
            } else {
                DPRINTF(DEBUG_LVL_WARN, "%u%% frequency slowdown detected on worker %lu. Within threshold (%u%%).\n", changePercentage, (u64)worker->id, FREQUENCY_SLOWDOWN_THRESHOLD);
                worker->active = true;
            }
        }
    } else {
        switch(msg->type & PD_MSG_TYPE_ONLY) {
        case PD_MSG_DB_FREE:
        {
            DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager invoke: [DB_FREE]\n");
            break;
        }
        case PD_MSG_WORK_DESTROY:
        {
            DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager invoke: [EDT_DESTROY]\n");
            break;
        }
        case PD_MSG_EVT_DESTROY:
        {
            DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager invoke: [EVT_DESTROY]\n");
            break;
        }
        case PD_MSG_DEP_SATISFY:
        {
            DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager invoke: [DEP_SATISFY]\n");
            break;
        }
        default:
            break;
        }
    }
    return 0;
}

u8 nullResiliencyNotify(ocrResiliency_t *self, ocrPolicyMsg_t *msg, u32 properties) {
    switch(msg->type & PD_MSG_TYPE_ONLY) {
    case PD_MSG_DB_CREATE:
    {
        DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager notify: [DB_CREATE]\n");
        break;
    }
    case PD_MSG_WORK_CREATE:
    {
        DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager notify: [EDT_CREATE]\n");
        break;
    }
    case PD_MSG_EVT_CREATE:
    {
        DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager notify: [EVT_CREATE]\n");
        break;
    }
    case PD_MSG_DEP_ADD:
    {
        DPRINTF(DEBUG_LVL_INFO, "Resiliency Manager notify: [DEP_ADD]\n");
        break;
    }
    default:
        break;
    }
    return 0;
}

u8 nullResiliencyRecover(ocrResiliency_t *self, ocrFaultCode_t faultCode, ocrLocation_t loc, u8 *buffer) {

    DPRINTF(DEBUG_LVL_WARN, "Resiliency Manager Fault Handler:......\n");
    switch(faultCode) {
    case OCR_FAULT_FREQUENCY:
        {
            u64 curFreq = *((u64*)buffer);
            ocrWorker_t *worker;
            getCurrentEnv(NULL, &worker, NULL, NULL);
            //if (worker->curFreq < WORKER_FREQ_NORMAL) {
                DPRINTF(DEBUG_LVL_WARN, "Frequency fault detected in worker %lu (new frequency %lu MHz)\n", (u64)loc, curFreq);
                worker->curFreq = curFreq;
            //}
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

/******************************************************/
/* OCR-NULL RESILIENCY FACTORY                        */
/******************************************************/

void destructResiliencyFactoryNull(ocrResiliencyFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrResiliencyFactory_t * newResiliencyFactoryNull(ocrParamList_t *perType) {
    ocrResiliencyFactory_t* base = (ocrResiliencyFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrResiliencyFactoryNull_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newResiliencyNull;
    base->destruct = &destructResiliencyFactoryNull;
    base->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrResiliency_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                 phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), nullResiliencySwitchRunlevel);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrResiliency_t*), nullResiliencyDestruct);
    base->fcts.invoke = FUNC_ADDR(u8 (*)(ocrResiliency_t*, ocrPolicyMsg_t*, u32), nullResiliencyInvoke);
    base->fcts.notify = FUNC_ADDR(u8 (*)(ocrResiliency_t*, ocrPolicyMsg_t*, u32), nullResiliencyNotify);
    base->fcts.recover = FUNC_ADDR(u8 (*)(ocrResiliency_t*, ocrFaultCode_t, ocrLocation_t, u8*), nullResiliencyRecover);

    return base;
}

#endif /* ENABLE_RESILIENCY_NULL */
