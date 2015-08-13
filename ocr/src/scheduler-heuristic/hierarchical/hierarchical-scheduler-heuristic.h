/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_SCHEDULER_HEURISTIC_H__
#define __HC_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_HIERARCHICAL

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* HC SCHEDULER_HEURISTIC                           */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextHierarchical_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    ocrSchedulerObject_t *mySchedulerObject_2;
    u64 stealSchedulerObjectIndex;        // Cached index of the deque lasted visited during steal attempts
#if 0 // Example fields for simulation mode
    ocrSchedulerObjectActionSet_t singleActionSet;
    ocrSchedulerObjectAction_t insertAction;
    ocrSchedulerObjectAction_t removeAction;
#endif
} ocrSchedulerHeuristicContextHierarchical_t;

typedef struct _ocrSchedulerHeuristicHierarchical_t {
    ocrSchedulerHeuristic_t base;
} ocrSchedulerHeuristicHierarchical_t;

/****************************************************/
/* HC SCHEDULER_HEURISTIC FACTORY                   */
/****************************************************/

typedef struct _paramListSchedulerHeuristicHierarchical_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicHierarchical_t;

typedef struct _ocrSchedulerHeuristicFactoryHierarchical_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryHierarchical_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryHierarchical(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_HIERARCHICAL */
#endif /* __HC_SCHEDULER_HEURISTIC_H__ */

