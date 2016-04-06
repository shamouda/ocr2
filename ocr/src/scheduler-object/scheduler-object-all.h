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
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
#include "scheduler-object/null/null-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
#include "scheduler-object/wst/wst-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
#include "scheduler-object/deq/deq-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
#include "scheduler-object/list/list-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
#include "scheduler-object/map/map-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
#include "scheduler-object/pdspace/pdspace-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
#include "scheduler-object/dbspace/dbspace-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
#include "scheduler-object/dbtime/dbtime-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
#include "scheduler-object/pr-wsh/pr-wsh-scheduler-object.h"
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
#include "scheduler-object/bin-heap/bin-heap-scheduler-object.h"
#endif

typedef enum _schedulerObjectType_t {
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    schedulerObjectNull_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    schedulerObjectWst_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    schedulerObjectDeq_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    schedulerObjectList_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    schedulerObjectMap_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
    schedulerObjectPdspace_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
    schedulerObjectDbspace_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
    schedulerObjectDbtime_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
    schedulerObjectPrWsh_id,
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    schedulerObjectBinHeap_id,
#endif
    schedulerObjectMax_id
} schedulerObjectType_t;

extern const char * schedulerObject_types[];

ocrSchedulerObjectFactory_t * newSchedulerObjectFactory(schedulerObjectType_t type, ocrParamList_t *perType);

#endif /* __SCHEDULER_OBJECT_ALL_H__ */
