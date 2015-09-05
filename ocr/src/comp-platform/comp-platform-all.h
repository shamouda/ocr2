/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMP_PLATFORM_ALL_H__
#define __COMP_PLATFORM_ALL_H__

#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-config.h"
#include "utils/ocr-utils.h"

typedef enum _compPlatformType_t {
#ifdef ENABLE_COMP_PLATFORM_PTHREAD
    compPlatformPthread_id,
#endif
#ifdef ENABLE_COMP_PLATFORM_FSIM
    compPlatformFsim_id,
#endif
    compPlatformMax_id,
} compPlatformType_t;

extern const char * compplatform_types[];

#ifdef ENABLE_COMP_PLATFORM_PTHREAD
#include "comp-platform/pthread/pthread-comp-platform.h"
#endif
#ifdef ENABLE_COMP_PLATFORM_FSIM
#include "comp-platform/fsim/fsim-comp-platform.h"
#endif

// Add other compute platforms using the same pattern as above

ocrCompPlatformFactory_t *newCompPlatformFactory(compPlatformType_t type, ocrParamList_t *typeArg);

#endif /* __COMP_PLATFORM_ALL_H__ */
