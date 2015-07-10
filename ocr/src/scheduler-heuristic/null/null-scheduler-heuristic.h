/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __NULL_SCHEDULER_HEURISTIC_H__
#define __NULL_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"

/****************************************************/
/* NULL SCHEDULER_HEURISTIC                                   */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextNull_t {
    ocrSchedulerHeuristicContext_t base;
} ocrSchedulerHeuristicContextNull_t;

typedef struct _ocrSchedulerHeuristicNull_t {
    ocrSchedulerHeuristic_t base;
} ocrSchedulerHeuristicNull_t;

/****************************************************/
/* NULL SCHEDULER_HEURISTIC FACTORY                           */
/****************************************************/

typedef struct _paramListSchedulerHeuristicNull_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicNull_t;

typedef struct _ocrSchedulerHeuristicFactoryNull_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryNull_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryNull(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_NULL */
#endif /* __NULL_SCHEDULER_HEURISTIC_H__ */

