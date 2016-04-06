/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __STATIC_SCHEDULER_HEURISTIC_H__
#define __STATIC_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_STATIC

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* STATIC SCHEDULER_HEURISTIC                       */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextStatic_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    ocrSchedulerObject_t *commSchedulerObject;  // The deque for the comm tasks
} ocrSchedulerHeuristicContextStatic_t;

typedef struct _ocrSchedulerHeuristicStatic_t {
    ocrSchedulerHeuristic_t base;
    volatile u64 rrCounter;                     // Counter for round-robin placement
    bool isDistributed;                         // Keep a flag to check if distributed
} ocrSchedulerHeuristicStatic_t;

/****************************************************/
/* STATIC SCHEDULER_HEURISTIC FACTORY               */
/****************************************************/

typedef struct _paramListSchedulerHeuristicStatic_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicStatic_t;

typedef struct _ocrSchedulerHeuristicFactoryStatic_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryStatic_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryStatic(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_STATIC */
#endif /* __STATIC_SCHEDULER_HEURISTIC_H__ */

