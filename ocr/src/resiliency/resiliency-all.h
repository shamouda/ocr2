/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __RESILIENCY_ALL_H__
#define __RESILIENCY_ALL_H__

#include "debug.h"
#include "ocr-resiliency.h"
#include "ocr-config.h"
#include "utils/ocr-utils.h"

typedef enum _resiliencyType_t {
#ifdef ENABLE_RESILIENCY_NULL
    resiliencyNull_id,
#endif
#ifdef ENABLE_RESILIENCY_X86
    resiliencyX86_id,
#endif
#ifdef ENABLE_RESILIENCY_TG
    resiliencyTG_id,
#endif
    resiliencyMax_id,
} resiliencyType_t;

extern const char * resiliency_types[];

#ifdef ENABLE_RESILIENCY_NULL
#include "resiliency/null/null-resiliency.h"
#endif
#ifdef ENABLE_RESILIENCY_X86
#include "resiliency/x86/x86-resiliency.h"
#endif
#ifdef ENABLE_RESILIENCY_TG
#include "resiliency/tg/tg-resiliency.h"
#endif

// Add other compute platforms using the same pattern as above

ocrResiliencyFactory_t *newResiliencyFactory(resiliencyType_t type, ocrParamList_t *typeArg);

#endif /* __RESILIENCY_ALL_H__ */
