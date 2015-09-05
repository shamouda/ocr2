/*
* This file is subject to the license agreement located in the file LICENSE
* and cannot be distributed without it. This notice cannot be
* removed or modified.
*/

#include "allocator/allocator-all.h"
#include "debug.h"
#ifdef ENABLE_VALGRIND
#include <valgrind/memcheck.h>
#include "ocr-hal.h"
#endif

#define DEBUG_TYPE ALLOCATOR

const char * allocator_types[] = {
#ifdef ENABLE_ALLOCATOR_SIMPLE
    "simple",
#endif
#ifdef ENABLE_ALLOCATOR_QUICK
    "quick",
#endif
#ifdef ENABLE_ALLOCATOR_TLSF
    "tlsf",
#endif
#ifdef ENABLE_ALLOCATOR_MALLOCPROXY
    "mallocproxy",
#endif
#ifdef ENABLE_ALLOCATOR_NULL
    "null",
#endif
    NULL
};

ocrAllocatorFactory_t *newAllocatorFactory(allocatorType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_ALLOCATOR_SIMPLE
    case allocatorSimple_id:
        return newAllocatorFactorySimple(typeArg);
#endif
#ifdef ENABLE_ALLOCATOR_QUICK
    case allocatorQuick_id:
        return newAllocatorFactoryQuick(typeArg);
#endif
#ifdef ENABLE_ALLOCATOR_TLSF
    case allocatorTlsf_id:
        return newAllocatorFactoryTlsf(typeArg);
#endif
#ifdef ENABLE_ALLOCATOR_MALLOCPROXY
    case allocatorMallocProxy_id:
        return newAllocatorFactoryMallocProxy(typeArg);
#endif
#ifdef ENABLE_ALLOCATOR_NULL
    case allocatorNull_id:
        return newAllocatorFactoryNull(typeArg);
#endif
    case allocatorMax_id:
    default:
        ASSERT(0); // Invalid allocator factory in configuration file
        return NULL;
    };
}

void initializeAllocatorOcr(ocrAllocatorFactory_t * factory, ocrAllocator_t * self, ocrParamList_t *perInstance) {
    self->fguid.guid = UNINITIALIZED_GUID;
    self->fguid.metaDataPtr = self;
    self->pd = NULL;

    self->fcts = factory->allocFcts;
    self->memories = NULL;
    self->memoryCount = 0;
}

void allocatorFreeFunction(void* blockPayloadAddr) {
    u8 * pPoolHeaderDescr = ((u8 *)(((u64) blockPayloadAddr)-sizeof(u64)));
    DPRINTF(DEBUG_LVL_VERB, "allocatorFreeFunction:  PoolHeaderDescr at 0x%lx is 0x%x\n",
        (u64) pPoolHeaderDescr, *pPoolHeaderDescr);
#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_DEFINED((u64) pPoolHeaderDescr, sizeof(u64));
#endif
    u8 poolHeaderDescr;
    GET8(poolHeaderDescr, (u64) pPoolHeaderDescr);
#ifdef ENABLE_VALGRIND
    VALGRIND_MAKE_MEM_NOACCESS((u64) pPoolHeaderDescr, sizeof(u64));
#endif
    u8 type = poolHeaderDescr & POOL_HEADER_TYPE_MASK;
    switch(type) {
#ifdef ENABLE_ALLOCATOR_TLSF
    case allocatorTlsf_id:
        tlsfDeallocate(blockPayloadAddr);
        return;
#endif
#ifdef ENABLE_ALLOCATOR_SIMPLE
    case allocatorSimple_id:
        simpleDeallocate(blockPayloadAddr);
        return;
#endif
#ifdef ENABLE_ALLOCATOR_QUICK
    case allocatorQuick_id:
        quickDeallocate(blockPayloadAddr);
        return;
#endif
#ifdef ENABLE_ALLOCATOR_MALLOCPROXY
    case allocatorMallocProxy_id:
        mallocProxyDeallocate(blockPayloadAddr);
        return;
#endif
    case allocatorMax_id:
    default:
        ASSERT(0); // Invalid allocator in configuration file
        return;
    };
}
