/**
 * @brief OCR low-level memory allocator
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __MEM_PLATFORM_ALL_H__
#define __MEM_PLATFORM_ALL_H__

#include "debug.h"
#include "ocr-config.h"
#include "ocr-mem-platform.h"
#include "utils/ocr-utils.h"

typedef enum _memPlatformType_t {
#ifdef ENABLE_MEM_PLATFORM_MALLOC
    memPlatformMalloc_id,
#endif
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC
    memPlatformNumaAlloc_id,
#endif
#ifdef ENABLE_MEM_PLATFORM_FSIM
    memPlatformFsim_id,
#endif
    memPlatformMax_id
} memPlatformType_t;

extern const char * memplatform_types[];

#ifdef ENABLE_MEM_PLATFORM_MALLOC
#include "mem-platform/malloc/malloc-mem-platform.h"
#endif
#ifdef ENABLE_MEM_PLATFORM_NUMA_ALLOC
#include "mem-platform/numa-alloc/numa-alloc-mem-platform.h"
#endif
#ifdef ENABLE_MEM_PLATFORM_FSIM
#include "mem-platform/fsim/fsim-mem-platform.h"
#endif

// Add other memory platforms using the same pattern as above

ocrMemPlatformFactory_t *newMemPlatformFactory(memPlatformType_t type, ocrParamList_t *typeArg);

#endif /* __MEM_PLATFORM_ALL_H__ */


