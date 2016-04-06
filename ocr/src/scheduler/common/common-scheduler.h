/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMMON_SCHEDULER_H__
#define __COMMON_SCHEDULER_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_COMMON

#include "ocr-scheduler.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/* Derived structures                                 */
/******************************************************/

#define MAX_SCHEDULER_HEURISTICS_COUNT 3

typedef struct {
    ocrSchedulerFactory_t base;
} ocrSchedulerFactoryCommon_t;

typedef struct {
    ocrScheduler_t scheduler;
    struct _ocrSchedulerHeuristic_t * schedulerHeuristics[MAX_SCHEDULER_HEURISTICS_COUNT];
} ocrSchedulerCommon_t;

// The CFG file passes a string that contains the identifiers for the scheduler heuristics
// to be used by the common-scheduler. The string is formatted as "comp0:plc1:comm2".
// The three heuristic kinds comp, plc and comm must be defined. The indices corresponds
// to the heuristics 'id' in the CFG and can be identical if the heuristic implementation
// is able to accomodate any or all of computation, placement and communication.
typedef struct _paramListSchedulerCommonInst_t {
    paramListSchedulerInst_t base;
    char * config; /**< The string to parse */
    ocrSchedulerHeuristic_t ** heuristics; /**< Array of heuristics instances as built by the configuration parser */
    // These represent the range of scheduling heuristics defined for this scheduler in the CFG file.
    // It is always a range: (1-1) or (1-2) and high bound is always inclusive.
    u32 heuristicIdLow;
    u32 heuristicIdHigh;
} paramListSchedulerCommonInst_t;

ocrSchedulerFactory_t * newOcrSchedulerFactoryCommon(ocrParamList_t *perType);

#endif /* ENABLE_SCHEDULER_COMMON */
#endif /* __COMMON_SCHEDULER_H__ */

