/**
 * @brief OCR memory targets
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __MEM_TARGET_ALL_H__
#define __MEM_TARGET_ALL_H__

#include "debug.h"
#include "ocr-config.h"
#include "ocr-mem-target.h"
#include "utils/ocr-utils.h"

typedef enum _memTargetType_t {
#ifdef ENABLE_MEM_TARGET_SHARED
    memTargetShared_id,
#endif
    memTargetMax_id
} memTargetType_t;

extern const char * memtarget_types[];

#ifdef ENABLE_MEM_TARGET_SHARED
#include "mem-target/shared/shared-mem-target.h"
#endif

// Add other memory targets using the same pattern as above

ocrMemTargetFactory_t *newMemTargetFactory(memTargetType_t type, ocrParamList_t *typeArg);

#endif /* __MEM_TARGET_ALL_H__ */
