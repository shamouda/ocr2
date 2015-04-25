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
//     u32 i = 0;
//     while(i < compTarget->platformCount) {
//         compTarget->platforms[i]->fcts.destruct(compTarget->platforms[i]);
//         ++i;
//     }
//     runtimeChunkFree((u64)(compTarget->platforms), NULL);
// #ifdef OCR_ENABLE_STATISTICS
//     statsCOMPTARGET_STOP(compTarget->pd, compTarget->fguid.guid, compTarget);
// #endif
//     runtimeChunkFree((u64)compTarget, NULL);
}

u8 ptSwitchRunlevel(ocrCompTarget_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                    u32 phase, u32 properties, void (*callback)(u64), u64 val) {

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
        // Nothing
        break;
    case RL_PD_OK:
        break;
    case RL_GUID_OK:
        // Nothing to do
        break;
    case RL_MEMORY_OK:
        // Nothing to do
        break;
    case RL_COMPUTE_OK:
        // We can allocate our map here because the memory is up
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}
#if 0
void ptBegin(ocrCompTarget_t * compTarget, ocrPolicyDomain_t * PD, ocrWorkerType_t workerType) {

    ASSERT(compTarget->platformCount == 1);
    compTarget->pd = PD;
    compTarget->platforms[0]->fcts.begin(compTarget->platforms[0], PD, workerType);
}

void ptStart(ocrCompTarget_t * compTarget, ocrPolicyDomain_t * PD, ocrWorker_t * worker) {
    // Get a GUID
    guidify(PD, (u64)compTarget, &(compTarget->fguid), OCR_GUID_COMPTARGET);
    compTarget->worker = worker;

#ifdef OCR_ENABLE_STATISTICS
    statsCOMPTARGET_START(PD, compTarget->fguid.guid, compTarget->fguid.metaDataPtr);
#endif

    ASSERT(compTarget->platformCount == 1);
    compTarget->platforms[0]->fcts.start(compTarget->platforms[0], PD, worker);
}

void ptStop(ocrCompTarget_t * compTarget, ocrRunLevel_t newRl, u32 action) {
    switch(newRl) {
        case RL_STOP: {
            ASSERT(compTarget->platformCount == 1);
            compTarget->platforms[0]->fcts.stop(compTarget->platforms[0], newRl, action);

            //TODO shouldn't this done in shutdown or deallocate ?
            // Destroy the GUID
            PD_MSG_STACK(msg);
            getCurrentEnv(NULL, NULL, NULL, &msg);

        #define PD_MSG (&msg)
        #define PD_TYPE PD_MSG_GUID_DESTROY
            msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = compTarget->fguid;
            PD_MSG_FIELD_I(properties) = 0;
            compTarget->pd->fcts.processMessage(compTarget->pd, &msg, false); // Don't really care about result
        #undef PD_MSG
        #undef PD_TYPE
            compTarget->fguid.guid = UNINITIALIZED_GUID;
            break;
        }
        case RL_SHUTDOWN: {
            ASSERT(compTarget->platformCount == 1);
            compTarget->platforms[0]->fcts.stop(compTarget->platforms[0], RL_SHUTDOWN, action);
            break;
        }
        case RL_DEALLOCATE: {
            u32 i = 0;
            while(i < compTarget->platformCount) {
                compTarget->platforms[0]->fcts.stop(compTarget->platforms[0], RL_DEALLOCATE, action);
                ++i;
            }
            runtimeChunkFree((u64)(compTarget->platforms), NULL);
        #ifdef OCR_ENABLE_STATISTICS
            statsCOMPTARGET_STOP(compTarget->pd, compTarget->fguid.guid, compTarget);
        #endif
            runtimeChunkFree((u64)compTarget, NULL);
            break;
        }

        default:
            ASSERT("Unknown runlevel in stop function");
    }
}
#endif

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
    runtimeChunkFree((u64)factory, NULL);
}

ocrCompTargetFactory_t *newCompTargetFactoryPt(ocrParamList_t *perType) {
    ocrCompTargetFactory_t *base = (ocrCompTargetFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCompTargetFactoryPt_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newCompTargetPt;
    base->initialize = &initializeCompTargetPt;
    base->destruct = &destructCompTargetFactoryPt;
    base->targetFcts.destruct = FUNC_ADDR(void (*)(ocrCompTarget_t*), ptDestruct);
    base->targetFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                       u32, u32, void (*)(u64), u64), ptSwitchRunlevel);
    base->targetFcts.getThrottle = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, u64*), ptGetThrottle);
    base->targetFcts.setThrottle = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, u64), ptSetThrottle);
    base->targetFcts.setCurrentEnv = FUNC_ADDR(u8 (*)(ocrCompTarget_t*, ocrPolicyDomain_t*, ocrWorker_t*), ptSetCurrentEnv);

    return base;
}

#endif /* ENABLE_COMP_TARGET_PT */
