/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __SCHEDULER_ALL_H__
#define __SCHEDULER_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"

#include "ocr-scheduler.h"
#ifdef ENABLE_SCHEDULER_COMMON
#include "scheduler/common/common-scheduler.h"
#endif
#ifdef ENABLE_SCHEDULER_HC
#include "scheduler/hc/hc-scheduler.h"
#endif
#ifdef ENABLE_SCHEDULER_HC_COMM_DELEGATE
#include "scheduler/hc-comm-delegate/hc-comm-delegate-scheduler.h"
#endif
#ifdef ENABLE_SCHEDULER_XE
#include "scheduler/xe/xe-scheduler.h"
#endif
#ifdef ENABLE_SCHEDULER_CE
#include "scheduler/ce/ce-scheduler.h"
#endif

typedef enum _schedulerType_t {
#ifdef ENABLE_SCHEDULER_COMMON
    schedulerCommon_id,
#endif
#ifdef ENABLE_SCHEDULER_HC
    schedulerHc_id,
#endif
#ifdef ENABLE_SCHEDULER_HC_COMM_DELEGATE
    schedulerHcCommDelegate_id,
#endif
#ifdef ENABLE_SCHEDULER_XE
    schedulerXe_id,
#endif
#ifdef ENABLE_SCHEDULER_CE
    schedulerCe_id,
#endif
    schedulerMax_id
} schedulerType_t;

extern const char * scheduler_types[];

ocrSchedulerFactory_t * newSchedulerFactory(schedulerType_t type, ocrParamList_t *perType);

#endif /* __SCHEDULER_ALL_H__ */
