/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_ALLOCATOR_NULL

#include "ocr-hal.h"
#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"
#include "null-allocator.h"

void nullDestruct(ocrAllocator_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 nullSwitchRunlevel(ocrAllocator_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        // Nothing to do (yes, we are an allocator but no setup is required)
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_ALLOCATOR);
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
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

void* nullAllocate(ocrAllocator_t *self, u64 size, u64 hints) {
    return NULL;
}

void nullDeallocate(ocrAllocator_t *self, void* address) {
}

void* nullReallocate(ocrAllocator_t *self, void* address, u64 size) {
    return NULL;
}

// Method to create the NULL allocator
ocrAllocator_t * newAllocatorNull(ocrAllocatorFactory_t * factory, ocrParamList_t *perInstance) {

    ocrAllocator_t *base = (ocrAllocator_t*) runtimeChunkAlloc(sizeof(ocrAllocatorNull_t), PERSISTENT_CHUNK);
    factory->initialize(factory, base, perInstance);
    return (ocrAllocator_t *) base;
}

void initializeAllocatorNull(ocrAllocatorFactory_t * factory, ocrAllocator_t * self, ocrParamList_t * perInstance) {
    initializeAllocatorOcr(factory, self, perInstance);
}

/******************************************************/
/* OCR ALLOCATOR NULL FACTORY                         */
/******************************************************/

static void destructAllocatorFactoryNull(ocrAllocatorFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrAllocatorFactory_t * newAllocatorFactoryNull(ocrParamList_t *perType) {
    ocrAllocatorFactory_t* base = (ocrAllocatorFactory_t*)
                                  runtimeChunkAlloc(sizeof(ocrAllocatorFactoryNull_t), NONPERSISTENT_CHUNK);
    ASSERT(base);
    base->instantiate = &newAllocatorNull;
    base->initialize = &initializeAllocatorNull;
    base->destruct =  &destructAllocatorFactoryNull;
    base->allocFcts.destruct = FUNC_ADDR(void (*)(ocrAllocator_t*), nullDestruct);
    base->allocFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrAllocator_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                      phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), nullSwitchRunlevel);
    base->allocFcts.allocate = FUNC_ADDR(void* (*)(ocrAllocator_t*, u64, u64), nullAllocate);
    //base->allocFcts.free = FUNC_ADDR(void (*)(ocrAllocator_t*, void*), nullDeallocate);
    base->allocFcts.reallocate = FUNC_ADDR(void* (*)(ocrAllocator_t*, void*, u64), nullReallocate);
    return base;
}

#endif /* ENABLE_NULL_ALLOCATOR */
