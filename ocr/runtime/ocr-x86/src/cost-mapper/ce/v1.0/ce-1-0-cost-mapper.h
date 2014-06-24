/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_1_0_COST_MAPPER_H__
#define __CE_1_0_COST_MAPPER_H__

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_CE_1_0

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "utils/ocr-utils.h"
#include "ocr-cost-mapper.h"
#include "ocr-component.h"

#define MAX_PRIORITY_LEVELS 4
// Priorities get clamped to the range [0 - MAX_PRIORITY_LEVELS)
#define PRIORITY_LEVEL(x) (x < MAX_PRIORITY_LEVELS ? x : MAX_PRIORITY_LEVELS - 1)

typedef struct {
    ocrCostMapperFactory_t base;
} ocrCostMapperFactoryCe_1_0_t;

typedef struct {
    ocrCostMapper_t base;
    ocrComponentFactory_t *componentFactory;
    ocrComponent_t *sharedComponent;                      /* component shared among all workers */
    ocrComponent_t **priorityLevels[MAX_PRIORITY_LEVELS]; /* Number of priority levels */
    u64 *priorityLevelCount;                              /* Number of components in each level */
    u32 *priorityLevelWorker;                             /* The current priority level of the worker */
    u32 numWorkers;                                       /* Number of workers managed by this cost mapper */
    u64 count;                                            /* total number of elements */
    bool *workerAvailable;                                /* Flag to test if a worker has components mapped to it */
    u32 *stealAttempts;                                   /* Worker specific counter for repelled steals */
    u32 maxStealRepels;                                   /* Total number of steal attempts that will fail before a steal is allowed */
    u64 maxLoad;                                          /* Maximum number of EDTs in the scheduler at any given time */
} ocrCostMapperCe_1_0_t;

typedef struct _paramListCostMapperCe_1_0Inst_t {
    paramListCostMapperInst_t base;
} paramListCostMapperCe_1_0Inst_t;

ocrCostMapperFactory_t * newOcrCostMapperFactoryCe(ocrParamList_t *perType);

#endif /* ENABLE_COST_MAPPER_CE_1_0 */
#endif /* __CE_1_0_COST_MAPPER_H__ */
