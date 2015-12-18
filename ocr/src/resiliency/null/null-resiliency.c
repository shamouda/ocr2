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

#include "policy-domain/hc/hc-policy.h"
#include "resiliency/null/null-resiliency.h"

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
    return OCR_ENOTSUP;
}

u8 nullResiliencyNotify(ocrResiliency_t *self, ocrPolicyMsg_t *msg, u32 properties) {
    return OCR_ENOTSUP;
}

u8 nullResiliencyRecover(ocrResiliency_t *self, ocrFaultCode_t faultCode, ocrLocation_t loc, u64 *buffer) {

    PRINTF("Fault detected on worker %lu ......\n", loc);
    if(faultCode == OCR_FAULT_FREQUENCY){
        PRINTF("\nAttempting to recover from fault code 0x%lx\n", faultCode);
        PRINTF("\nNew freqeuncy detected. Need to tranistion to %lu mHz\n\n", buffer[0]);

        //TODO -Need to update and sync new frequency. Not yet supported
        //     -Need to restart worker state to retry current EDT.  Not yet supported
        //
        //     Normal Pause Wakeup for now (No-op)
        ocrPolicyDomain_t *pd;
        getCurrentEnv(&pd, NULL, NULL, NULL);
        ocrPolicyDomainHc_t *rself = (ocrPolicyDomainHc_t *)pd;

        if(hal_cmpswap32((u32 *)&rself->pqrFlags.runtimePause, true, false) == true){
            hal_xadd32((u32 *)&rself->pqrFlags.pauseCounter, -1);
        }

        //SUCCESS
        PRINTF("Successfully recovered; continuing app execution...\n\n");
        return 0;

    //Only testing for FREQUENCY fault for now
    }else{
        return OCR_ENOTSUP;
    }

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
