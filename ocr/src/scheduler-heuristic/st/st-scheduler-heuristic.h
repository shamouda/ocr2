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
/* ST SCHEDULER_HEURISTIC SPECIFIC STRUCTURES       */
/****************************************************/

/* The EDT proxy is used by the scheduler node to keep
 * track of EDTs that are blocked from carrying out the
 * analysis of space/time heuristic because their dependences
 * have not yet arrived at the scheduler node. Once they arrive,
 * the analysis is resumed for the EDT.
 */
typedef struct _ocrEdtProxy_t {
    ocrGuid_t edtGuid;          // The guid of the EDT being analyzed
    ocrLocation_t edtLocation;  // The location where the EDT is currently placed
    ocrEdtDep_t *depv;          // The dependence vector of the EDT
    u32 depc;                   // The number of dependences of the EDT
    u32 deps;                   // The number of useful dependences of the EDT (not counting NULL_GUID etc).
    u32 frontierIdx;            // Suspension point in the dependence vector
} ocrEdtProxy_t;

/****************************************************/
/* ST SCHEDULER_HEURISTIC                           */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextSt_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    u64 stealSchedulerObjectIndex;              // Cached index of the deque lasted visited during steal attempts
    ocrSchedulerObjectIterator_t *mapIterator;  // Preallocated map iterator
    ocrSchedulerObjectIterator_t *listIterator; // Preallocated reusable list iterator
} ocrSchedulerHeuristicContextSt_t;

typedef struct _ocrSchedulerHeuristicSt_t {
    ocrSchedulerHeuristic_t base;
    ocrLocation_t schedulerLocation;            // The node location where the scheduling analysis is done on behalf of this node.
    ocrLocation_t locationPlacement;            // Location where last EDT was placed
    volatile u32 locationLock;                  // Lock to make round-robin decision
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

