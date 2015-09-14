/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __ST_SCHEDULER_HEURISTIC_H__
#define __ST_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_ST

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* ST SCHEDULER_HEURISTIC                           */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextSt_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    u64 stealSchedulerObjectIndex;              // Cached index of the deque lasted visited during steal attempts
    ocrSchedulerObjectIterator_t *mapIterator;  // Preallocated map iterator
} ocrSchedulerHeuristicContextSt_t;

typedef struct _ocrSchedulerHeuristicSt_t {
    ocrSchedulerHeuristic_t base;
    ocrLocation_t schedulerLocation;            // The node location where the scheduling analysis is done on behalf of this node.
} ocrSchedulerHeuristicSt_t;

/****************************************************/
/* ST SCHEDULER_HEURISTIC FACTORY                   */
/****************************************************/

typedef struct _paramListSchedulerHeuristicSt_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicSt_t;

typedef struct _ocrSchedulerHeuristicFactorySt_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactorySt_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactorySt(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_ST */
#endif /* __ST_SCHEDULER_HEURISTIC_H__ */

