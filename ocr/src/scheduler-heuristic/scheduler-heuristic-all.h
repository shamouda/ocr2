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
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC
#include "scheduler-heuristic/hc/hc-scheduler-heuristic.h"
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_PC
#include "scheduler-heuristic/pc/pc-scheduler-heuristic.h"
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE
#include "scheduler-heuristic/ce/ce-scheduler-heuristic.h"
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL
#include "scheduler-heuristic/null/null-scheduler-heuristic.h"
#endif

typedef enum _schedulerHeuristicType_t {
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC
    schedulerHeuristicHc_id,
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_PC
    schedulerHeuristicPc_id,
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE
    schedulerHeuristicCe_id,
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL
    schedulerHeuristicNull_id,
#endif
    schedulerHeuristicMax_id
} schedulerHeuristicType_t;

extern const char * schedulerHeuristic_types[];

ocrSchedulerHeuristicFactory_t * newSchedulerHeuristicFactory(schedulerHeuristicType_t type, ocrParamList_t *perType);

#endif /* __SCHEDULER_HEURISTIC_ALL_H__ */
