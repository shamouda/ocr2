/**
 * @brief Simple implementation of a numa-based alloc wrapper
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "ocr-config.h"
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC

#include "ocr-hal.h"
#include "debug.h"
#include "utils/rangeTracker.h"
#include "ocr-sysboot.h"
#include "ocr-types.h"
#include "ocr-mem-platform.h"
#include "ocr-policy-domain.h"
#include "mem-platform/numa-alloc/numa-alloc-mem-platform.h"

#include <stdlib.h>
#include <string.h>

// Poor man's basic lock
#define INIT_LOCK(addr) do {*addr = 0;} while(0);
#define LOCK(addr) do { hal_lock32(addr); } while(0);
#define UNLOCK(addr) do { hal_unlock32(addr); } while(0);

/******************************************************/
/* OCR MEM PLATFORM NUMA_ALLOC IMPLEMENTATION             */
/******************************************************/

void numaAllocDestruct(ocrMemPlatform_t *self) {
    // BUG #673: Deal with objects owned by multiple PDs
    //runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

// BUG #673: This mem-platform may be shared by multiple threads (for example
// one SPAD shared by 2 CEs. We therefore do the numaAlloc/free and what not extremely early
// on so that only the NODE_MASTER does it in a race free manner.
u8 numaAllocSwitchRunlevel(ocrMemPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        // This should ideally be in MEMORY_OK
        // NOTE: This is serial because only thread is up until PD_OK
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_NETWORK_OK, phase)) {
            if(self->startAddr != 0ULL)
                break; // We break out early since we are already initialized
            // This is where we need to update the memory
            // using the sysboot functions
            ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t*)self;
            // 1. Check if NUMA is available
            ASSERT(numa_available() != -1);
            // 2. Check if the node number is reasonable
            ASSERT(rself->numa_node <= numa_max_node());
            self->startAddr = (u64)numa_alloc_onnode(self->size, rself->numa_node);
            // Check that the mem-platform size in config file is reasonable
            ASSERT(self->startAddr);
            self->endAddr = self->startAddr + self->size;

            // rangeTracker will be located at self->startAddr, and it should be zero'ed
            // since initializeRange() assumes zero-ed 'lock' and 'inited' variables
            ASSERT(self->size >= MEM_PLATFORM_ZEROED_AREA_SIZE);    // make sure no buffer overrun
            // zero beginning part to cover rangeTracker and pad, and allocator metadata part i.e. pool header (pool_t)
            memset((void *)self->startAddr , 0, MEM_PLATFORM_ZEROED_AREA_SIZE);

            rself->pRangeTracker = initializeRange(
                16, self->startAddr, self->endAddr, USER_FREE_TAG);
        } else if((properties & RL_TEAR_DOWN) && RL_IS_LAST_PHASE_DOWN(PD, RL_NETWORK_OK, phase)) {
            // This is also serial because after PD_OK we are down to one thread
            ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t*)self;
            // The first guy through here does this
            if(self->startAddr != 0ULL) {
                if(rself->pRangeTracker)    // in case of numaAllocproxy, pRangeTracker==0
                    destroyRange(rself->pRangeTracker);
                // Here we can free the memory we allocated
                numa_free((void*)(self->startAddr), self->size);
                self->startAddr = 0ULL;
            }
        }
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            // We can now set our PD (before this, we couldn't because
            // "our" PD might not have been started
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        // Should ideally do what's in NETWORK_OK
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

u8 numaAllocGetThrottle(ocrMemPlatform_t *self, u64 *value) {
    return 1; // Not supported
}

u8 numaAllocSetThrottle(ocrMemPlatform_t *self, u64 value) {
    return 1; // Not supported
}

void numaAllocGetRange(ocrMemPlatform_t *self, u64* startAddr,
                    u64 *endAddr) {
    if(startAddr) *startAddr = self->startAddr;
    if(endAddr) *endAddr = self->endAddr;
}

u8 numaAllocChunkAndTag(ocrMemPlatform_t *self, u64 *startAddr, u64 size,
                     ocrMemoryTag_t oldTag, ocrMemoryTag_t newTag) {

    if(oldTag >= MAX_TAG || newTag >= MAX_TAG)
        return 3;

    ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t *)self;

    u64 iterate = 0;
    u64 startRange, endRange;
    u8 result;
    LOCK(&(rself->pRangeTracker->lockChunkAndTag));
    // first check if there's existing one. (query part)
    do {
        result = getRegionWithTag(rself->pRangeTracker, newTag, &startRange,
                                  &endRange, &iterate);
        if(result == 0 && endRange - startRange >= size) {
            *startAddr = startRange;
//            printf("ChunkAndTag returning (existing) start of 0x%"PRIx64" for size %"PRId64" (0x%"PRIx64") Tag %"PRId32"\n",
//                    *startAddr, size, size, newTag);
            // exit.
            UNLOCK(&(rself->pRangeTracker->lockChunkAndTag));
            return result;
        }
    } while(result == 0);



    // now do chunkAndTag (allocation part)
    iterate = 0;
    do {
        result = getRegionWithTag(rself->pRangeTracker, oldTag, &startRange,
                                  &endRange, &iterate);
        if(result == 0 && endRange - startRange >= size) {
            // This is a fit, we do not look for "best" fit for now
            *startAddr = startRange;
//            printf("ChunkAndTag returning start of 0x%"PRIx64" for size %"PRId64" (0x%"PRIx64") and newTag %"PRId32"\n",
//                    *startAddr, size, size, newTag);
            RESULT_ASSERT(splitRange(rself->pRangeTracker,
                                     startRange, size, newTag, 0), ==, 0);
            break;
        } else {
            if(result == 0) {
//                printf("ChunkAndTag, found [0x%"PRIx64"; 0x%"PRIx64"[ but too small for size %"PRId64" (0xllx)\n",
//                        startRange, endRange, size, size);
            }
        }
    } while(result == 0);

    UNLOCK(&(rself->pRangeTracker->lockChunkAndTag));
    return result;
}

u8 numaAllocTag(ocrMemPlatform_t *self, u64 startAddr, u64 endAddr,
             ocrMemoryTag_t newTag) {

    if(newTag >= MAX_TAG)
        return 3;

    ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t *)self;

    LOCK(&(rself->lock));
    RESULT_ASSERT(splitRange(rself->pRangeTracker, startAddr,
                             endAddr - startAddr, newTag, 0), ==, 0);
    UNLOCK(&(rself->lock));
    return 0;
}

u8 numaAllocQueryTag(ocrMemPlatform_t *self, u64 *start, u64* end,
                  ocrMemoryTag_t *resultTag, u64 addr) {
    ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t *)self;

    RESULT_ASSERT(getTag(rself->pRangeTracker, addr, start, end, resultTag),
                  ==, 0);
    return 0;
}

ocrMemPlatform_t* newMemPlatformNumaAlloc(ocrMemPlatformFactory_t * factory,
                                       ocrParamList_t *perInstance) {

    ocrMemPlatform_t *result = (ocrMemPlatform_t*)
                               runtimeChunkAlloc(sizeof(ocrMemPlatformNumaAlloc_t), PERSISTENT_CHUNK);
    factory->initialize(factory, result, perInstance);
    return result;
}

void initializeMemPlatformNumaAlloc(ocrMemPlatformFactory_t * factory, ocrMemPlatform_t * result, ocrParamList_t * perInstance) {
    initializeMemPlatformOcr(factory, result, perInstance);
    ocrMemPlatformNumaAlloc_t *rself = (ocrMemPlatformNumaAlloc_t*)result;
    rself->numa_node = ((paramListMemPlatformInst_t *)perInstance)->numa_node;
    INIT_LOCK(&(rself->lock));
}

/******************************************************/
/* OCR MEM PLATFORM NUMA_ALLOC FACTORY                    */
/******************************************************/

void destructMemPlatformFactoryNumaAlloc(ocrMemPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrMemPlatformFactory_t *newMemPlatformFactoryNumaAlloc(ocrParamList_t *perType) {
    ocrMemPlatformFactory_t *base = (ocrMemPlatformFactory_t*)
                                    runtimeChunkAlloc(sizeof(ocrMemPlatformFactoryNumaAlloc_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newMemPlatformNumaAlloc;
    base->initialize = &initializeMemPlatformNumaAlloc;
    base->destruct = &destructMemPlatformFactoryNumaAlloc;
    base->platformFcts.destruct = FUNC_ADDR(void (*) (ocrMemPlatform_t *), numaAllocDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrMemPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), numaAllocSwitchRunlevel);
    base->platformFcts.getThrottle = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *), numaAllocGetThrottle);
    base->platformFcts.setThrottle = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64), numaAllocSetThrottle);
    base->platformFcts.getRange = FUNC_ADDR(void (*) (ocrMemPlatform_t *, u64 *, u64 *), numaAllocGetRange);
    base->platformFcts.chunkAndTag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *, u64, ocrMemoryTag_t, ocrMemoryTag_t), numaAllocChunkAndTag);
    base->platformFcts.tag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64, u64, ocrMemoryTag_t), numaAllocTag);
    base->platformFcts.queryTag = FUNC_ADDR(u8 (*) (ocrMemPlatform_t *, u64 *, u64 *, ocrMemoryTag_t *, u64), numaAllocQueryTag);
    return base;
}

#endif /* ENABLE_MEM_PLATFORM_NUMA_ALLOC */
