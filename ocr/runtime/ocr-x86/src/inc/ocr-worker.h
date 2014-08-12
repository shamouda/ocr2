/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_WORKER_H__
#define __OCR_WORKER_H__

#include "ocr-comp-target.h"
#include "ocr-runtime-types.h"
#include "ocr-scheduler.h"
#include "ocr-types.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

struct _ocrPolicyDomain_t;
struct _ocrPolicyMsg_t;
struct _ocrMsgHandle_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListWorkerFact_t {
    ocrParamList_t base;
} paramListWorkerFact_t;

typedef struct _paramListWorkerInst_t {
    ocrParamList_t base;
} paramListWorkerInst_t;

/******************************************************/
/* OCR WORKER                                         */
/******************************************************/

struct _ocrWorker_t;
struct _ocrTask_t;

typedef struct _ocrWorkerFcts_t {
    void (*destruct)(struct _ocrWorker_t *self);

    void (*begin)(struct _ocrWorker_t *self, struct _ocrPolicyDomain_t * PD);

    /** @brief Start Worker
     */
    void (*start)(struct _ocrWorker_t *self, struct _ocrPolicyDomain_t * PD);

    /** @brief Run Worker
     */
    void* (*run)(struct _ocrWorker_t *self);

    /** @brief Query the worker to finish its work
     */
    void (*finish)(struct _ocrWorker_t *self);

    /** @brief Stop Worker
     */
    void (*stop)(struct _ocrWorker_t *self);

    /** @brief Check if Worker is still running
     *  @return true if the Worker is running, false otherwise
     */
    bool (*isRunning)(struct _ocrWorker_t *self);
} ocrWorkerFcts_t;

typedef struct _ocrDataParallelContext_t {
    ocrGuid_t task;        /* Guid of the data parallel task */
    ocrGuid_t latch;       /* Latch event that satisfies at the end of the computation */
    volatile u64 lb, ub;   /* Lower/Upper bounds of the iteration range of the data parallel computation performed by this worker */
    volatile u32 lock;     /* Lock to coordinate range stealing among multiple workers */
    volatile bool active;  /* Flag to indicate a running data parallel computation */
    u64 curIndex;          /* Index of the current iteration being performed by the worker */
    u64 id;
} ocrDpCtxt_t;

typedef struct _ocrWorker_t {
    ocrFatGuid_t fguid;
    struct _ocrPolicyDomain_t *pd;
    ocrLocation_t location;
    ocrWorkerType_t type;
#ifdef OCR_ENABLE_STATISTICS
    ocrStatsProcess_t *statProcess;
#endif

    ocrCompTarget_t **computes; /**< Compute node(s) associated with this worker */
    u64 computeCount;           /**< Number of compute node(s) associated */

    struct _ocrTask_t *curTask; /**< Currently executing task */
    struct _ocrDataParallelContext_t dpCtxt; /**< Data parallel context */
    ocrWorkerFcts_t fcts;
} ocrWorker_t;


/****************************************************/
/* OCR WORKER FACTORY                               */
/****************************************************/

typedef struct _ocrWorkerFactory_t {
    ocrWorker_t* (*instantiate) (struct _ocrWorkerFactory_t * factory,
                                 ocrParamList_t *perInstance);
    void (*initialize) (struct _ocrWorkerFactory_t * factory, struct _ocrWorker_t * worker, ocrParamList_t *perInstance);

    void (*destruct)(struct _ocrWorkerFactory_t * factory);
    ocrWorkerFcts_t workerFcts;
} ocrWorkerFactory_t;

void initializeWorkerOcr(ocrWorkerFactory_t * factory, ocrWorker_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_WORKER_H__ */
