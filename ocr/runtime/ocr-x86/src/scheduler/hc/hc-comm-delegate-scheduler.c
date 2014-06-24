/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HC_COMM_DELEGATE

#include "debug.h"
#include "ocr-sysboot.h"
#include "utils/deque.h"
#include "worker/hc/hc-worker.h"
#include "scheduler/hc/hc-comm-delegate-scheduler.h"
#include "comm-api/delegate/delegate-comm-api.h"

#define DEBUG_TYPE SCHEDULER

/**
 * Extensions to the HC scheduler implementation to handle communications.
 *
 * comp-workers produce and give communication work to the scheduler
 * while comm-workers take and consume communication work.
 *
 * IMPL: There is one outbox deque per worker. Communication workers can
 *       steal handlers from other workers outbox.
 */
 void hcCommSchedulerStart(ocrScheduler_t * self, ocrPolicyDomain_t * pd) {
    ocrSchedulerHcCommDelegate_t * commSched = (ocrSchedulerHcCommDelegate_t *) self;
    commSched->baseStart(self, pd);
    //Create outbox queues for each worker
    u64 boxCount = pd->workerCount;
    commSched->outboxesCount = boxCount;
    commSched->outboxes = pd->fcts.pdMalloc(pd, sizeof(deque_t *) * boxCount);
    u64 i;
    for(i = 0; i < boxCount; ++i) {
        commSched->outboxes[i] = newWorkStealingDeque(pd, NULL);
    }
    //Create inbox queues for each worker
    commSched->inboxesCount = boxCount;
    commSched->inboxes = pd->fcts.pdMalloc(pd, sizeof(deque_t *) * boxCount);
    for(i = 0; i < boxCount; ++i) {
        commSched->inboxes[i] = newWorkStealingDeque(pd, NULL);
    }
}

void hcCommSchedulerStop(ocrScheduler_t * self) {
    // deallocate all pdMalloc-ed structures
    ocrPolicyDomain_t * pd = self->pd;
    ocrSchedulerHcCommDelegate_t * commSched = (ocrSchedulerHcCommDelegate_t *) self;
    u64 i;
    for(i = 0; i < commSched->outboxesCount; ++i) {
        pd->fcts.pdFree(pd, commSched->outboxes[i]);
    }
    for(i = 0; i < commSched->inboxesCount; ++i) {
        pd->fcts.pdFree(pd, commSched->inboxes[i]);
    }
    pd->fcts.pdFree(pd, commSched->outboxes);
    pd->fcts.pdFree(pd, commSched->inboxes);
    commSched->baseStop(self);
}

/**
 * @brief Take communication work
 *
 * The scheduler must identify the type of the worker doing the call to
 * determine what to return.
 * In the case of comp-worker, a take returns a handle that has completed, while
 * for comm-worker a take returns a handle representing a message to be sent out.
 */
u8 hcCommSchedulerTakeComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t * fatHandlers, u32 properties) {
    ocrSchedulerHcCommDelegate_t * commSched = (ocrSchedulerHcCommDelegate_t *) self;
    ocrWorker_t *worker = NULL; //DIST-TODO sep-concern: Do we need a way to register worker types somehow ?
    getCurrentEnv(NULL, &worker, NULL, NULL);
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;

    if (hcWorker->hcType == HC_WORKER_COMM) {
        // Steal from other worker's outbox
        //PERF: use real randomized iterator here
        // Try a round of stealing on other worker's outbox
        //NOTE: If we don't care about wasting a steal on self, we wouldn't need
        //      the whole worker ID business
        //NOTE: It's debatable whether we should steal a bunch from each outbox
        //      or go over all outboxes
        deque_t ** outboxes = commSched->outboxes;
        u64 outboxesCount = commSched->outboxesCount;
        u64 wid = hcWorker->id;
        u64 i = (wid+1); // skip over 'self' outbox
        u32 success = 0;
        while ((i < (wid+outboxesCount)) && (success < *count)) {
            deque_t * deque = outboxes[i%outboxesCount];
            ocrMsgHandle_t* handle = deque->popFromHead(deque, 1);
            if (handle != NULL) {
                fatHandlers[success].metaDataPtr = handle;
                success++;
            }
            i++;
        }
        *count = success;
    } else {
        ASSERT(hcWorker->hcType == HC_WORKER_COMP);
        // Pop from own inbox
        u64 wid = hcWorker->id;
        deque_t * inbox = commSched->inboxes[wid];
        ocrMsgHandle_t* handle = NULL;
        u32 success = 0;
        do {
            // 'steal' from own inbox, comm-worker pushes to it
            handle = (ocrMsgHandle_t *) inbox->popFromHead(inbox, 0);
            if (handle == NULL) {
                break;
            }
            fatHandlers[success].metaDataPtr = handle;
            success++;
        } while (success < *count);
        *count = success;
    }
    return 0;
}

/**
 * @brief Called by comm and comp workers to give communication work.
 *
 * The scheduler must identify the type of the worker doing the call to
 * determine what to do.
 */
u8 hcCommSchedulerGiveComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* fatHandlers, u32 properties) {
    ocrSchedulerHcCommDelegate_t * commSched = (ocrSchedulerHcCommDelegate_t *) self;
    ocrWorker_t *worker = NULL; //DIST-TODO sep-concern: Do we need a way to register worker types somehow ?
    getCurrentEnv(NULL, &worker, NULL, NULL);
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;

    if (hcWorker->hcType == HC_WORKER_COMM) {
        // Comm-worker giving back to a worker's inbox
        u32 i=0;
        while (i < *count) {
            delegateMsgHandle_t* handle = (delegateMsgHandle_t *) fatHandlers[i].metaDataPtr;
            DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-delegate-scheduler:: Comm-worker pushes at tail of box %d\n",
                (int) self->pd->myLocation, handle->boxId);
            deque_t * inbox = commSched->inboxes[handle->boxId];
            inbox->pushAtTail(inbox, handle, 0);
            i++;
        }
        *count = i;
    } else {
        // Comp-worker giving a message to send
        u32 i=0;
        while (i < *count) {
            // Set delegate handle's box id.
            delegateMsgHandle_t* delHandle = (delegateMsgHandle_t *) fatHandlers[i].metaDataPtr;
            //DIST-TODO: boxId is defined in del-handle however only the scheduler is using it
            delHandle->boxId = hcWorker->id;
            DPRINTF(DEBUG_LVL_VVERB,"[%d] hc-comm-delegate-scheduler:: Comp-worker pushes at tail of box %d\n",
                (int) self->pd->myLocation, delHandle->boxId);
            ASSERT((delHandle->boxId >= 0) && (delHandle->boxId < self->pd->workerCount));
            // Put handle to worker's outbox
            deque_t * outbox = commSched->outboxes[delHandle->boxId];
            outbox->pushAtTail(outbox, (ocrMsgHandle_t *) delHandle, 0);
            i++;
        }
        *count = i;
    }
    return 0;
}

ocrScheduler_t* newSchedulerHcComm(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* base = (ocrScheduler_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHcCommDelegate_t), NULL);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeSchedulerHcComm(ocrSchedulerFactory_t * factory, ocrScheduler_t *self, ocrParamList_t *perInstance) {
    ocrSchedulerFactoryHcComm_t * derivedFactory = (ocrSchedulerFactoryHcComm_t *) factory;
    // Initialize the base scheduler
    derivedFactory->baseInitialize(factory, self, perInstance);
    ocrSchedulerHcCommDelegate_t * commSched = (ocrSchedulerHcCommDelegate_t *) self;
    commSched->outboxesCount = 0;
    commSched->outboxes = NULL;
    commSched->inboxesCount = 0;
    commSched->inboxes = NULL;
    commSched->baseStart = derivedFactory->baseStart;
    commSched->baseStop = derivedFactory->baseStop;
}

void destructSchedulerFactoryHcComm(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryHcCommDelegate(ocrParamList_t *perType) {
    // Create a base factory to get a hold at parent's impl function pointers.
    ocrSchedulerFactory_t * baseFactory = newOcrSchedulerFactoryHc(perType);
    ocrSchedulerFcts_t baseFcts = baseFactory->schedulerFcts;
    ocrSchedulerFactoryHcComm_t* derived = (ocrSchedulerFactoryHcComm_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryHcComm_t), (void *)1);

    // Store function pointers we need from the base implementation
    derived->baseInitialize = baseFactory->initialize;
    derived->baseStart = baseFcts.start;
    derived->baseStop = baseFcts.stop;

    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) derived;
    base->instantiate = FUNC_ADDR(ocrScheduler_t* (*)(ocrSchedulerFactory_t*, ocrParamList_t*), newSchedulerHcComm);
    base->initialize = FUNC_ADDR(void (*)(ocrSchedulerFactory_t*, ocrScheduler_t*, ocrParamList_t*), initializeSchedulerHcComm);
    base->destruct = FUNC_ADDR(void (*)(ocrSchedulerFactory_t*), destructSchedulerFactoryHcComm);
    // Copy base's function pointers
    base->schedulerFcts = baseFcts;
    // and specialize some
    base->schedulerFcts.start = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), hcCommSchedulerStart);
    base->schedulerFcts.stop  = FUNC_ADDR(void (*)(ocrScheduler_t*), hcCommSchedulerStop);
    base->schedulerFcts.takeComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), hcCommSchedulerTakeComm);
    base->schedulerFcts.giveComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), hcCommSchedulerGiveComm);

    baseFactory->destruct(baseFactory);
    return base;
}

#endif /* ENABLE_SCHEDULER_HC_COMM_DELEGATE */