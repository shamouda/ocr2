/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __SCHEDULER_OBJECT_ALL_H__
#define __SCHEDULER_OBJECT_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"

#include "ocr-scheduler-object.h"
#ifdef ENABLE_SCHEDULER_OBJECT_WST
#include "scheduler-object/wst/wst-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
#include "scheduler-object/deq/deq-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
#include "scheduler-object/null/null-scheduler-object.h"
#endif

typedef enum _schedulerObjectType_t {
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    schedulerObjectWst_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    schedulerObjectDeq_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    schedulerObjectNull_id,
#endif
    schedulerObjectMax_id
} schedulerObjectType_t;

extern const char * schedulerObject_types[];

ocrSchedulerObjectFactory_t * newSchedulerObjectFactory(schedulerObjectType_t type, ocrParamList_t *perType);

#endif /* __SCHEDULER_OBJECT_ALL_H__ */
