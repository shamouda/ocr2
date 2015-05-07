/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_NULL

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"
#include "null-comm-platform.h"

#define DEBUG_TYPE COMM_PLATFORM

void nullCommDestruct (ocrCommPlatform_t * base) {
    runtimeChunkFree((u64)base, PERSISTENT_CHUNK);
}

u8 nullSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
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

u8 nullCommSetMaxExpectedMessageSize(ocrCommPlatform_t *self, u64 size, u32 mask) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 nullCommSendMessage(ocrCommPlatform_t *self, ocrLocation_t target,
                       ocrPolicyMsg_t *message, u64 *id,
                       u32 properties, u32 mask) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 nullCommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                       u32 properties, u32 *mask) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 nullCommWaitMessage(ocrCommPlatform_t *self,  ocrPolicyMsg_t **msg,
                       u32 properties, u32 *mask) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 nullCommDestructMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t *msg) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

ocrCommPlatform_t* newCommPlatformNull(ocrCommPlatformFactory_t *factory,
                                       ocrParamList_t *perInstance) {

    ocrCommPlatformNull_t * commPlatformNull = (ocrCommPlatformNull_t*)
            runtimeChunkAlloc(sizeof(ocrCommPlatformNull_t), PERSISTENT_CHUNK);
    ocrCommPlatform_t * derived = (ocrCommPlatform_t *) commPlatformNull;
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeCommPlatformNull(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * derived, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, derived, perInstance);
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCommPlatformFactoryNull(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrCommPlatformFactory_t *newCommPlatformFactoryNull(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryNull_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newCommPlatformNull;
    base->initialize = &initializeCommPlatformNull;
    base->destruct = &destructCommPlatformFactoryNull;

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), nullCommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), nullSwitchRunlevel);
    base->platformFcts.setMaxExpectedMessageSize = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, u64, u32),
                                                             nullCommSetMaxExpectedMessageSize);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrLocation_t,
                                                      ocrPolicyMsg_t *, u64*, u32, u32), nullCommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               nullCommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               nullCommWaitMessage);
    base->platformFcts.destructMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t*),
                                                   nullCommDestructMessage);


    return base;
}
#endif /* ENABLE_COMM_PLATFORM_NULL */
