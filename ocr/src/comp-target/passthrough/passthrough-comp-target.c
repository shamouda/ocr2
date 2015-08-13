/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMP_TARGET_PASSTHROUGH

#include "comp-target/passthrough/passthrough-comp-target.h"
#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-comp-target.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif


void ptDestruct(ocrCompTarget_t *compTarget) {
    u32 i = 0;
    while(i < compTarget->platformCount) {
        compTarget->platforms[i]->fcts.destruct(compTarget->platforms[i]);
        ++i;
    }
    runtimeChunkFree((u64)(compTarget->platforms), PERSISTENT_CHUNK);
#ifdef OCR_ENABLE_STATISTICS
    statsCOMPTARGET_STOP(compTarget->pd, compTarget->fguid.guid, compTarget);
#endif
    runtimeChunkFree((u64)compTarget, PERSISTENT_CHUNK);
}

u8 ptSwitchRunlevel(ocrCompTarget_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                    phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // The worker is the capable module and we operate as
    // inert wrt it
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    // Call the runlevel change on the underlying platform
    if(runlevel == RL_CONFIG_PARSE && (properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
        // Set the worker properly the first time
        ASSERT(self->platformCount == 1);
    }
    if(properties & RL_BRING_UP) {
        toReturn |= self->platforms[0]->fcts.switchRunlevel(self->platforms[0], PD, runlevel, phase, properties,
                                                            callback, val);
    }

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP)
            self->pd = PD;
        self->platforms[0]->worker = self->worker;
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_COMPTARGET);
            }
        } else {
            // Tear-down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
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
            }
        }
        break;
// BUG #225 Where should this go?
#ifdef OCR_ENABLE_STATISTICS
        statsCOMPTARGET_START(PD, compTarget->fguid.guid, compTarget->fguid.metaDataPtr);
#endif
    case RL_USER_OK:
        break;
    default:
        ASSERT(0);
    }
    if(properties & RL_TEAR_DOWN) {
        toReturn |= self->platforms[0]->fcts.switchRunlevel(self->platforms[0], PD, runlevel, phase, properties,
                                                            NULL, 0);
    }
    return toReturn;
}

u8 ptGetThrottle(ocrCompTarget_t *compTarget, u64 *value) {
    ASSERT(compTarget->platformCount == 1);
    return compTarget->platforms[0]->fcts.getThrottle(compTarget->platforms[0], value);
}

u8 ptSetThrottle(ocrCompTarget_t *compTarget, u64 value) {
    ASSERT(compTarget->platformCount == 1);
    return compTarget->platforms[0]->fcts.setThrottle(compTarget->platforms[0], value);
}

u8 ptSetCurrentEnv(ocrCompTarget_t *compTarget, ocrPolicyDomain_t *pd,
                   ocrWorker_t *worker) {

    ASSERT(compTarget->platformCount == 1);
    return compTarget->platforms[0]->fcts.setCurrentEnv(compTarget->platforms[0], pd, worker);
}

ocrCompTarget_t * newCompTargetPt(ocrCompTargetFactory_t * factory,
                                  ocrParamList_t* perInstance) {
    ocrCompTargetPt_t * compTarget = (ocrCompTargetPt_t*)runtimeChunkAlloc(sizeof(ocrCompTargetPt_t), PERSISTENT_CHUNK);
    ocrCompTarget_t *derived = (ocrCompTarget_t *)compTarget;
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeCompTargetPt(ocrCompTargetFactory_t * factory, ocrCompTarget_t * derived, ocrParamList_t * perInstance) {
    initializeCompTargetOcr(factory, derived, perInstance);
}

/******************************************************/
/* OCR COMP TARGET HC FACTORY                         */
/******************************************************/
static void destructCompTargetFactoryPt(ocrCompTargetFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrCompTargetFactory_t *newCompTargetFactoryPt(ocrParamList_t *perType) {
    ocrCompTargetFactory_t *base = (ocrCompTargetFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCompTargetFactoryPt_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newCompTargetPt;
    base->initialize = &initializeCompTargetPt;
    base->destruct = &destructCompTargetFactoryPt;
    base->targetFcts.destruct = FUNC_ADDR(void (*)(ocrCompTarget_t*), ptDestruct);
    base->targetFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                       phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), ptSwitchRunlevel);
    base->targetFcts.getThrottle = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, u64*), ptGetThrottle);
    base->targetFcts.setThrottle = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, u64), ptSetThrottle);
    base->targetFcts.setCurrentEnv = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, ocrPolicyDomain_t*, ocrWorker_t*), ptSetCurrentEnv);

    return base;
}

#endif /* ENABLE_COMP_TARGET_PT */
