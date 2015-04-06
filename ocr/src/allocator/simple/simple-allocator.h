/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __ALLOCATOR_SIMPLE_H__
#define __ALLOCATOR_SIMPLE_H__

#include "ocr-config.h"
#ifdef ENABLE_ALLOCATOR_SIMPLE

#include "ocr-allocator.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

typedef struct {
    ocrAllocatorFactory_t base;
} ocrAllocatorFactorySimple_t;

typedef struct {
    ocrAllocator_t base;
    u8  poolStorageOffset;  // Distance from poolAddr to storage address of the pool (which wasn't necessarily 8-byte aligned).
    u8  poolStorageSuffix;  // Bytes at end of storage space not usable for the pool.
    volatile u64 poolAddr;  // Address of the 8-byte-aligned net pool storage space.
    u64 poolSize;
} ocrAllocatorSimple_t;

typedef struct {
    paramListAllocatorInst_t base;
} paramListAllocatorSimple_t;

extern ocrAllocatorFactory_t* newAllocatorFactorySimple(ocrParamList_t *perType);

void simpleDeallocate(void* address);

typedef struct _pool {
    u64 *pool_start;
    u64 *pool_end;
    u64 *freelist;
    u32 lock;
    u32 inited;
} pool_t;

#endif /* ENABLE_ALLOCATOR_SIMPLE */
#endif /* __SIMPLE_ALLOCATOR_H__ */
