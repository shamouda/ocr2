/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_SCHEDULER_H__
#define __HC_SCHEDULER_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HC

#include "ocr-scheduler.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

typedef struct {
    ocrSchedulerFactory_t base;
} ocrSchedulerFactoryHc_t;

typedef struct {
    ocrScheduler_t scheduler;
} ocrSchedulerHc_t;

typedef struct _paramListSchedulerHcInst_t {
    paramListSchedulerInst_t base;
} paramListSchedulerHcInst_t;

ocrSchedulerFactory_t * newOcrSchedulerFactoryHc(ocrParamList_t *perType);

#endif /* ENABLE_SCHEDULER_HC */
#endif /* __HC_SCHEDULER_H__ */

