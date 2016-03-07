/*
* This file is subject to the license agreement located in the file LICENSE
* and cannot be distributed without it. This notice cannot be
* removed or modified.
*/

#include "mem-platform/mem-platform-all.h"
#include "debug.h"

const char * memplatform_types[] = {
#ifdef ENABLE_MEM_PLATFORM_MALLOC
    "malloc",
#endif
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC
    "numa_alloc",
#endif
#ifdef ENABLE_MEM_PLATFORM_FSIM
    "fsim",
#endif
    NULL
};

ocrMemPlatformFactory_t *newMemPlatformFactory(memPlatformType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_MEM_PLATFORM_MALLOC
    case memPlatformMalloc_id:
        return newMemPlatformFactoryMalloc(typeArg);
#endif
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC
    case memPlatformNumaAlloc_id:
        return newMemPlatformFactoryNumaAlloc(typeArg);
#endif
#ifdef ENABLE_MEM_PLATFORM_FSIM
    case memPlatformFsim_id:
        return newMemPlatformFactoryFsim(typeArg);
#endif
    default:
        ASSERT(0);
        return NULL;
    };
}

void initializeMemPlatformOcr(ocrMemPlatformFactory_t * factory, ocrMemPlatform_t * self, ocrParamList_t *perInstance) {
    self->pd = NULL;
    self->fcts = factory->platformFcts;
    self->size = ((paramListMemPlatformInst_t *)perInstance)->size;
    self->startAddr = self->endAddr = 0ULL;
}
