/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PLACEMENT_AFFINITY_SCHEDULER_HEURISTIC_H__
#define __PLACEMENT_AFFINITY_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_PLACEMENT_AFFINITY

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "experimental/ocr-platform-model.h"

/****************************************************/
/* PLACEMENT AFFINITY SCHEDULER HEURISTIC           */
/****************************************************/

typedef struct _ocrSchedulerHeuristicContextPlacementAffinity_t {
    ocrSchedulerHeuristicContext_t base;
} ocrSchedulerHeuristicContextPlacementAffinity_t;

typedef struct _ocrSchedulerHeuristicPlacementAffinity_t {
    ocrSchedulerHeuristic_t base;
    u32 lock; /**< Lock for updating edtLastPlacementIndex information */
    u64 edtLastPlacementIndex; /**< Index of the last guid returned for an edt */
    ocrPlatformModelAffinity_t * platformModel; // Cached from PD
    ocrLocation_t myLocation; // Cached from PD
} ocrSchedulerHeuristicPlacementAffinity_t;

/****************************************************/
/* PLACEMENT AFFINITY FACTORY                       */
/****************************************************/

typedef struct _paramListSchedulerHeuristicPlacementAffinity_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicPlacementAffinity_t;

typedef struct _ocrSchedulerHeuristicFactoryPlacementAffinity_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryPlacementAffinity_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryPlacementAffinity(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_PLACEMENT_AFFINITY */
#endif /* __PLACEMENT_AFFINITY_SCHEDULER_HEURISTIC_H__ */

