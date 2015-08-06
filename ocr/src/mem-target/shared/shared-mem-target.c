/**
 * @brief Simple implementation of a shared memory target
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_MEM_TARGET_SHARED

#include "debug.h"
#include "mem-target/shared/shared-mem-target.h"
#include "ocr-mem-platform.h"
#include "ocr-mem-target.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif


#define DEBUG_TYPE MEM_TARGET
/******************************************************/
/* OCR MEM TARGET SHARED IMPLEMENTATION               */
/******************************************************/

void sharedDestruct(ocrMemTarget_t *self) {
    ASSERT(self->memoryCount == 1);
    self->memories[0]->fcts.destruct(self->memories[0]);
    // BUG #673: Deal with objects that are shared across PDs.
    /*
    runtimeChunkFree((u64)self->memories, PERSISTENT_CHUNK);
#ifdef OCR_ENABLE_STATISTICS
    statsMEMTARGET_STOP(self->pd, self->fguid.guid, self->fguid.metaDataPtr);
#endif
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
    */
}

u8 sharedSwitchRunlevel(ocrMemTarget_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                        phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    ASSERT(self->memoryCount == 1);
    if(properties & RL_BRING_UP) {
        toReturn |= self->memories[0]->fcts.switchRunlevel(
            self->memories[0], PD, runlevel, phase, properties, NULL, self->level);
    }
    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        // Nothing
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            // We can now set our PD (before this, we couldn't because
            // "our" PD might not have been started
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        // BUG #673: We do not GUIDIFY due to a race if multiple PDs use the same
        // memory
        /*
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
            guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_MEMTARGET);
            DPRINTF(DEBUG_LVL_VERB, "SharedStart PD=%p\n", self->pd);
#ifdef OCR_ENABLE_STATISTICS
            statsMEMTARGET_START(PD, self->fguid.guid, self->fguid.metaDataPtr);
#endif
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
            PD_MSG_STACK(msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
            msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = self->fguid;
            PD_MSG_FIELD_I(properties) = 0;
            toReturn |= self->pd->fcts.processMessage(
                self->pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
            self->fguid.guid = NULL_GUID;
        }
        */
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    if(properties & RL_TEAR_DOWN) {
        toReturn |= self->memories[0]->fcts.switchRunlevel(
            self->memories[0], PD, runlevel, phase, properties, NULL, 0);
    }
    return toReturn;
}

u8 sharedGetThrottle(ocrMemTarget_t *self, u64* value) {
    return 1;
}

u8 sharedSetThrottle(ocrMemTarget_t *self, u64 value) {
    return 1;
}

void sharedGetRange(ocrMemTarget_t *self, u64* startAddr,
                    u64* endAddr) {
    return self->memories[0]->fcts.getRange(
               self->memories[0], startAddr, endAddr);
}

u8 sharedChunkAndTag(ocrMemTarget_t *self, u64 *startAddr,
                     u64 size, ocrMemoryTag_t oldTag, ocrMemoryTag_t newTag) {

    return self->memories[0]->fcts.chunkAndTag(
               self->memories[0], startAddr, size, oldTag, newTag);
}

u8 sharedTag(ocrMemTarget_t *self, u64 startAddr, u64 endAddr,
             ocrMemoryTag_t newTag) {

    return self->memories[0]->fcts.tag(self->memories[0], startAddr,
                                       endAddr, newTag);
}

u8 sharedQueryTag(ocrMemTarget_t *self, u64 *start, u64 *end,
                  ocrMemoryTag_t *resultTag, u64 addr) {

    return self->memories[0]->fcts.queryTag(self->memories[0], start,
                                            end, resultTag, addr);
}

ocrMemTarget_t* newMemTargetShared(ocrMemTargetFactory_t * factory,
                                   ocrParamList_t *perInstance) {

    ocrMemTarget_t *result = (ocrMemTarget_t*)
                             runtimeChunkAlloc(sizeof(ocrMemTargetShared_t), PERSISTENT_CHUNK);
    factory->initialize(factory, result, perInstance);
    return result;
}

void initializeMemTargetShared(ocrMemTargetFactory_t * factory, ocrMemTarget_t * result, ocrParamList_t * perInstance) {
    initializeMemTargetOcr(factory, result, perInstance);
}

/******************************************************/
/* OCR MEM TARGET SHARED FACTORY                    */
/******************************************************/

static void destructMemTargetFactoryShared(ocrMemTargetFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrMemTargetFactory_t *newMemTargetFactoryShared(ocrParamList_t *perType) {
    ocrMemTargetFactory_t *base = (ocrMemTargetFactory_t*)
        runtimeChunkAlloc(sizeof(ocrMemTargetFactoryShared_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newMemTargetShared;
    base->initialize = &initializeMemTargetShared;
    base->destruct = &destructMemTargetFactoryShared;
    base->targetFcts.destruct = FUNC_ADDR(void (*)(ocrMemTarget_t*), sharedDestruct);
    base->targetFcts.switchRunlevel = FUNC_ADDR(
        u8 (*)(ocrMemTarget_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
               phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), sharedSwitchRunlevel);
    base->targetFcts.getThrottle = FUNC_ADDR(u8 (*)(ocrMemTarget_t*, u64*), sharedGetThrottle);
    base->targetFcts.setThrottle = FUNC_ADDR(u8 (*)(ocrMemTarget_t*, u64), sharedSetThrottle);
    base->targetFcts.getRange = FUNC_ADDR(void (*)(ocrMemTarget_t*, u64*, u64*), sharedGetRange);
    base->targetFcts.chunkAndTag = FUNC_ADDR(u8 (*)(ocrMemTarget_t*, u64*, u64, ocrMemoryTag_t, ocrMemoryTag_t), sharedChunkAndTag);
    base->targetFcts.tag = FUNC_ADDR(u8 (*)(ocrMemTarget_t*, u64, u64, ocrMemoryTag_t), sharedTag);
    base->targetFcts.queryTag = FUNC_ADDR(u8 (*)(ocrMemTarget_t*, u64*, u64*, ocrMemoryTag_t*, u64), sharedQueryTag);

    return base;
}

#endif /* ENABLE_MEM_TARGET_SHARED */
