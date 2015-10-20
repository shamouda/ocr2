/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __ALLOCATOR_QUICK_H__
#define __ALLOCATOR_QUICK_H__

#include "ocr-config.h"
#ifdef ENABLE_ALLOCATOR_QUICK

#include "ocr-allocator.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

typedef struct {
    ocrAllocatorFactory_t base;
} ocrAllocatorFactoryQuick_t;

typedef struct {
    ocrAllocator_t base;
    volatile u64 poolAddr;  // Address of the 8-byte-aligned net pool storage space.
    u64 poolSize;
    u8  poolStorageOffset;  // Distance from poolAddr to storage address of the pool (which wasn't necessarily 8-byte aligned).
    u8  poolStorageSuffix;  // Bytes at end of storage space not usable for the pool.
} ocrAllocatorQuick_t;

typedef struct {
    paramListAllocatorInst_t base;
} paramListAllocatorQuick_t;

extern ocrAllocatorFactory_t* newAllocatorFactoryQuick(ocrParamList_t *perType);

void quickDeallocate(void* address);

#endif /* ENABLE_ALLOCATOR_QUICK */
#endif /* __QUICK_ALLOCATOR_H__ */
