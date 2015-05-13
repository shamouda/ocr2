/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __MEM_PLATFORM_FSIM_H__
#define __MEM_PLATFORM_FSIM_H__

#include "ocr-config.h"
#ifdef ENABLE_MEM_PLATFORM_FSIM

#include "debug.h"
#include "utils/rangeTracker.h"
#include "ocr-mem-platform.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

typedef struct {
    ocrMemPlatformFactory_t base;
} ocrMemPlatformFactoryFsim_t;

typedef struct {
    ocrMemPlatform_t base;
    rangeTracker_t *pRangeTracker;
/* ocrMemPlatformFsim_t (and rangeTracker, too) exists in every L1. i.e. multiple copies and multiple locks.
 * So, locking does not work. What I did is to move rangeTracker structure into each memory level, not L1,
 * so that there would be a single lock variable for each memory it manages.
 */
    u32 lock;   // this does not work on fsim, so I'll use pRangeTracker->lockChunkAndTag instead. See Bug #497
} ocrMemPlatformFsim_t;

typedef struct {
    paramListMemPlatformInst_t base;
    u64 start;
} paramListMemPlatformFsim_t;

extern ocrMemPlatformFactory_t* newMemPlatformFactoryFsim(ocrParamList_t *perType);

#endif /* ENABLE_MEM_PLATFORM_FSIM */
#endif /* __MEM_PLATFORM_FSIM_H__ */
