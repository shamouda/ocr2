/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMMON_SCHEDULER_H__
#define __COMMON_SCHEDULER_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_COMMON

#include "ocr-scheduler.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/* Derived structures                                 */
/******************************************************/

typedef struct {
    ocrSchedulerFactory_t base;
} ocrSchedulerFactoryCommon_t;

typedef struct {
    ocrScheduler_t scheduler;
} ocrSchedulerCommon_t;

typedef struct _paramListSchedulerCommonInst_t {
    paramListSchedulerInst_t base;
} paramListSchedulerCommonInst_t;

ocrSchedulerFactory_t * newOcrSchedulerFactoryCommon(ocrParamList_t *perType);

#endif /* ENABLE_SCHEDULER_COMMON */
#endif /* __COMMON_SCHEDULER_H__ */

