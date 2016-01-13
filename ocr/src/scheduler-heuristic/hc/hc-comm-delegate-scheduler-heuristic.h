/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_COMM_DELEGATE_SCHEDULER_HEURISTIC_H__
#define __HC_COMM_DELEGATE_SCHEDULER_HEURISTIC_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC_COMM_DELEGATE

#include "ocr-scheduler-heuristic.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-scheduler-object.h"
#include "utils/deque.h"
#include "utils/list.h"

/****************************************************/
/* HC COMM DELEGATE SCHEDULER_HEURISTIC             */
/****************************************************/

// Cached information about context
typedef struct _ocrSchedulerHeuristicContextHcCommDelegate_t {
    ocrSchedulerHeuristicContext_t base;
    ocrSchedulerObject_t *mySchedulerObject;    // The deque owned by a specific worker (context)
    u64 stealSchedulerObjectIndex;              // Cached index of the deque lasted visited during steal attempts
} ocrSchedulerHeuristicContextHcCommDelegate_t;

typedef struct _hcCommDelWorkpileIterator_t {
    struct _ocrWorkpile_t **workpiles;
    u64 id, curr, mod;
} hcCommDelWorkpileIterator_t;

typedef struct _ocrSchedulerHeuristicHcCommDelegate_t {
    ocrSchedulerHeuristic_t base;
    u64 outboxesCount;
    deque_t ** outboxes;
    u64 inboxesCount;
    deque_t ** inboxes;
} ocrSchedulerHeuristicHcCommDelegate_t;

/****************************************************/
/* HC COMM DELEGATE SCHEDULER_HEURISTIC FACTORY     */
/****************************************************/

typedef struct _paramListSchedulerHeuristicHcCommDelegate_t {
    paramListSchedulerHeuristic_t base;
} paramListSchedulerHeuristicHcCommDelegate_t;

typedef struct _ocrSchedulerHeuristicFactoryHcCommDelegate_t {
    ocrSchedulerHeuristicFactory_t base;
} ocrSchedulerHeuristicFactoryHcCommDelegate_t;

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryHcCommDelegate(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_HEURISTIC_HC_COMM_DELEGATE */
#endif /* __HC_COMM_DELEGATE_SCHEDULER_HEURISTIC_H__ */

