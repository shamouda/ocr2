/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PRIORITY_SCHEDULER_HEURISTIC_H__
#define __PRIORITY_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_PRIORITY

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* PRIORITY SCHEDULER_HEURISTIC                           */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextPriority_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
#if 0 // Example fields for simulation mode
    ocrSchedulerObjectActionSet_t singleActionSet;
    ocrSchedulerObjectAction_t insertAction;
    ocrSchedulerObjectAction_t removeAction;
#endif
} ocrSchedulerHeuristicContextPriority_t;

typedef struct _ocrSchedulerHeuristicPriority_t {
    ocrSchedulerHeuristic_t base;
} ocrSchedulerHeuristicPriority_t;

/****************************************************/
/* PRIORITY SCHEDULER_HEURISTIC FACTORY                   */
/****************************************************/

typedef struct _paramListSchedulerHeuristicPriority_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicPriority_t;

typedef struct _ocrSchedulerHeuristicFactoryPriority_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryPriority_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryPriority(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_PRIORITY */
#endif /* __PRIORITY_SCHEDULER_HEURISTIC_H__ */

