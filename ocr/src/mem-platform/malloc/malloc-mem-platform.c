/**
 * @brief Simple implementation of a malloc wrapper
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "ocr-config.h"
#ifdef ENABLE_MEM_PLATFORM_MALLOC

#include "ocr-hal.h"
#include "debug.h"
#include "utils/rangeTracker.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"
#include "ocr-mem-platform.h"
#include "ocr-policy-domain.h"
#include "mem-platform/malloc/malloc-mem-platform.h"

#include <stdlib.h>

// Poor man's basic lock
#define INIT_LOCK(addr) do {*addr = 0;} while(0);
#define LOCK(addr) do { hal_lock32(addr); } while(0);
#define UNLOCK(addr) do { hal_unlock32(addr); } while(0);

/******************************************************/
/* OCR MEM PLATFORM MALLOC IMPLEMENTATION             */
/******************************************************/

void mallocDestruct(ocrMemPlatform_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 mallocSwitchRunlevel(ocrMemPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        if(properties & RL_BRING_UP) {
            // We can now set our PD (before this, we couldn't because
            // "our" PD might not have been started
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
            // This is where we need to update the memory
            // using the sysboot functions
            self->startAddr = (u64)malloc(self->size);
            // Check that the mem-platform size in config file is reasonable
            ASSERT(self->startAddr);
            self->endAddr = self->startAddr + self->size;

            ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t*)self;
            rself->pRangeTracker = initializeRange(
                16, self->startAddr, self->endAddr, USER_FREE_TAG);
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
            ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t*)self;
            if(rself->pRangeTracker)    // in case of mallocproxy, pRangeTracker==0
                destroyRange(rself->pRangeTracker);
            // Here we can free the memory we allocated
            free((void*)(self->startAddr));
        }
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

u8 mallocGetThrottle(ocrMemPlatform_t *self, u64 *value) {
    return 1; // Not supported
}

u8 mallocSetThrottle(ocrMemPlatform_t *self, u64 value) {
    return 1; // Not supported
}

void mallocGetRange(ocrMemPlatform_t *self, u64* startAddr,
                    u64 *endAddr) {
    if(startAddr) *startAddr = self->startAddr;
    if(endAddr) *endAddr = self->endAddr;
}

u8 mallocChunkAndTag(ocrMemPlatform_t *self, u64 *startAddr, u64 size,
                     ocrMemoryTag_t oldTag, ocrMemoryTag_t newTag) {

    if(oldTag >= MAX_TAG || newTag >= MAX_TAG)
        return 3;

    ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t *)self;

    u64 iterate = 0;
    u64 startRange, endRange;
    u8 result;
    LOCK(&(rself->lock));
    do {
        result = getRegionWithTag(rself->pRangeTracker, oldTag, &startRange,
                                  &endRange, &iterate);
        if(result == 0 && endRange - startRange >= size) {
            // This is a fit, we do not look for "best" fit for now
            *startAddr = startRange;
            RESULT_ASSERT(splitRange(rself->pRangeTracker,
                                     startRange, size, newTag, 0), ==, 0);
            break;
        }
    } while(result == 0);

    UNLOCK(&(rself->lock));
    return result;
}

u8 mallocTag(ocrMemPlatform_t *self, u64 startAddr, u64 endAddr,
             ocrMemoryTag_t newTag) {

    if(newTag >= MAX_TAG)
        return 3;

    ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t *)self;

    LOCK(&(rself->lock));
    RESULT_ASSERT(splitRange(rself->pRangeTracker, startAddr,
                             endAddr - startAddr, newTag, 0), ==, 0);
    UNLOCK(&(rself->lock));
    return 0;
}

u8 mallocQueryTag(ocrMemPlatform_t *self, u64 *start, u64* end,
                  ocrMemoryTag_t *resultTag, u64 addr) {
    ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t *)self;

    RESULT_ASSERT(getTag(rself->pRangeTracker, addr, start, end, resultTag),
                  ==, 0);
    return 0;
}

ocrMemPlatform_t* newMemPlatformMalloc(ocrMemPlatformFactory_t * factory,
                                       ocrParamList_t *perInstance) {

    // TODO: This will be replaced by the runtime/GUID meta-data allocator
    // For now, we cheat and use good-old malloc which is kind of counter productive with
    // all the trouble we are going through to *not* use malloc...
    ocrMemPlatform_t *result = (ocrMemPlatform_t*)
                               runtimeChunkAlloc(sizeof(ocrMemPlatformMalloc_t), PERSISTENT_CHUNK);
    factory->initialize(factory, result, perInstance);
    return result;
}

void initializeMemPlatformMalloc(ocrMemPlatformFactory_t * factory, ocrMemPlatform_t * result, ocrParamList_t * perInstance) {
    initializeMemPlatformOcr(factory, result, perInstance);
    ocrMemPlatformMalloc_t *rself = (ocrMemPlatformMalloc_t*)result;
    INIT_LOCK(&(rself->lock));
}

/******************************************************/
/* OCR MEM PLATFORM MALLOC FACTORY                    */
/******************************************************/

void destructMemPlatformFactoryMalloc(ocrMemPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrMemPlatformFactory_t *newMemPlatformFactoryMalloc(ocrParamList_t *perType) {
    ocrMemPlatformFactory_t *base = (ocrMemPlatformFactory_t*)
                                    runtimeChunkAlloc(sizeof(ocrMemPlatformFactoryMalloc_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newMemPlatformMalloc;
    base->initialize = &initializeMemPlatformMalloc;
    base->destruct = &destructMemPlatformFactoryMalloc;
    base->platformFcts.destruct = FUNC_ADDR(void (*) (ocrMemPlatform_t *), mallocDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrMemPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), mallocSwitchRunlevel);
    base->platformFcts.getThrottle = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *), mallocGetThrottle);
    base->platformFcts.setThrottle = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64), mallocSetThrottle);
    base->platformFcts.getRange = FUNC_ADDR(void (*) (ocrMemPlatform_t *, u64 *, u64 *), mallocGetRange);
    base->platformFcts.chunkAndTag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *, u64, ocrMemoryTag_t, ocrMemoryTag_t), mallocChunkAndTag);
    base->platformFcts.tag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64, u64, ocrMemoryTag_t), mallocTag);
    base->platformFcts.queryTag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *, u64 *, ocrMemoryTag_t *, u64), mallocQueryTag);
    return base;
}

#endif /* ENABLE_MEM_PLATFORM_MALLOC */
