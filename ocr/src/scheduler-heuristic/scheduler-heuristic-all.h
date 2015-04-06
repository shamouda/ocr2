/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __SCHEDULER_HEURISTIC_ALL_H__
#define __SCHEDULER_HEURISTIC_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"

#include "ocr-scheduler-heuristic.h"
#include "scheduler-heuristic/hc/hc-scheduler-heuristic.h"
#include "scheduler-heuristic/null/null-scheduler-heuristic.h"

typedef enum _schedulerHeuristicType_t {
    schedulerHeuristicHc_id,
    schedulerHeuristicHcComm_id,
    schedulerHeuristicXe_id,
    schedulerHeuristicCe_id,
    schedulerHeuristicNull_id,
    schedulerHeuristicMax_id
} schedulerHeuristicType_t;

extern const char * schedulerHeuristic_types[];

ocrSchedulerHeuristicFactory_t * newSchedulerHeuristicFactory(schedulerHeuristicType_t type, ocrParamList_t *perType);

#endif /* __SCHEDULER_HEURISTIC_ALL_H__ */
