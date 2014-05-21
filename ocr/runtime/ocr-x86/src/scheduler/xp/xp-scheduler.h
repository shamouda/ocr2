/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __XP_SCHEDULER_H__
#define __XP_SCHEDULER_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_XP

#include "ocr-scheduler.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/* Support structures (workpile iterator)             */
/******************************************************/

struct _ocrWorkpile_t;

typedef struct _xpThreadWorkpileIterator_t {
    struct _ocrWorkpile_t **workpiles;
    u64 coreId;
    u64 currCore;
    u64 coreMod;
    u64 coreCount;
} xpThreadWorkpileIterator_t;

typedef struct {
    ocrSchedulerFactory_t base;
} ocrSchedulerFactoryXp_t;

typedef struct {
    ocrScheduler_t scheduler;
    xpThreadWorkpileIterator_t * stealIterators;
    u64 workerIdFirst;
    u64 coreCount;
    u64 threadsPerCore;
} ocrSchedulerXp_t;

typedef struct _paramListSchedulerXpInst_t {
    paramListSchedulerInst_t base;
    u64 workerIdFirst;
    u64 coreCount;
    u64 threadsPerCore;
} paramListSchedulerXpInst_t;

ocrSchedulerFactory_t * newOcrSchedulerFactoryXp(ocrParamList_t *perType);

#endif /* ENABLE_SCHEDULER_XP */
#endif /* __XP_SCHEDULER_H__ */

