/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PC_SCHEDULER_HEURISTIC_H__
#define __PC_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_PC

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* PC SCHEDULER_HEURISTIC                           */
/****************************************************/

typedef struct _ocrSchedulerHeuristicContextPc_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;	// The deque owned by a specific worker (context)
    u64 stealSchedulerObjectIndex;              // Cached index of the deque lasted visited during steal attempts
    ocrSchedulerObject_t *currentDbNode;        // DB node being processed
    ocrSchedulerObjectIterator_t *listIterator; // Preallocated list iterator
    ocrSchedulerObjectIterator_t *mapIterator;  // Preallocated map iterator
} ocrSchedulerHeuristicContextPc_t;

typedef struct _ocrSchedulerHeuristicPc_t {
    ocrSchedulerHeuristic_t base;
    ocrLocation_t analyzeLocation;              // location where the analysis is done
} ocrSchedulerHeuristicPc_t;

/****************************************************/
/* PC SCHEDULER_HEURISTIC FACTORY                   */
/****************************************************/

typedef struct _paramListSchedulerHeuristicPc_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicPc_t;

typedef struct _ocrSchedulerHeuristicFactoryPc_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryPc_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryPc(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_PC */
#endif /* __PC_SCHEDULER_HEURISTIC_H__ */

