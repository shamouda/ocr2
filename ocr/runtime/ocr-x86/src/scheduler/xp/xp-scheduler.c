/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#define _POSIX_C_SOURCE 200000
#include <time.h>
#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_XP

#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "scheduler/xp/xp-scheduler.h"
// TODO: This relies on data in hc-worker (its ID to do the mapping)
// This is non-portable (HC scheduler does not work with non
// HC worker) but works for now
#include "worker/hc/hc-worker.h" // NON PORTABLE FOR NOW

#define MAX_STEAL_TIMEOUT 10e3

/******************************************************/
/* OCR-Xeon Phi steal iterator                   */
/******************************************************/

static inline void xpThreadWorkpileIteratorReset(xpThreadWorkpileIterator_t *base) {
    base->currCore = (base->coreId + 1) % base->coreMod;
    base->coreCount = base->coreMod - 1;
}

static inline bool xpThreadWorkpileIteratorHasNext(xpThreadWorkpileIterator_t * base) {
    return base->coreCount;
}

static inline ocrWorkpile_t *xpThreadWorkpileIteratorNext(xpThreadWorkpileIterator_t * base) {

    u64 current = base->currCore;
    ocrWorkpile_t * toBeReturned = base->workpiles[current];
    base->currCore = (current+1) % base->coreMod;
    base->coreCount--;
     return toBeReturned;
}

static inline void xpInitWorkpileIterator(xpThreadWorkpileIterator_t *base, u64 coreId
                                        , u64 coreMod, ocrWorkpile_t **workpiles) {

    base->workpiles = workpiles;
    base->coreId = coreId;
    base->coreMod = coreMod;

    // The 'curr' field is initialized by reset
    xpThreadWorkpileIteratorReset(base);
}



/******************************************************/
/* OCR-XP SCHEDULER                                   */
/******************************************************/


static inline ocrWorkpile_t * popMappingOneToOne (ocrScheduler_t* base, u64 workerId ) {

    ocrSchedulerXp_t* derived = (ocrSchedulerXp_t*)base;
    u64 idx = (workerId - derived->workerIdFirst); // we always start workerIdFirst = 0 so we dont this this part
    idx = idx / derived->threadsPerCore;
//printf("popping on %lu\n",idx);
    return base->workpiles[idx];
}

static inline ocrWorkpile_t * pushMappingOneToOne (ocrScheduler_t* base, u64 workerId ) {

    ocrSchedulerXp_t* derived = (ocrSchedulerXp_t*)base;
    u64 idx = (workerId - derived->workerIdFirst); // we always start workerIdFirst = 0 so we dont this this part
    idx = idx / derived->threadsPerCore;

//printf("pushing on %lu\n",idx);
    return base->workpiles[idx];
}

static inline xpThreadWorkpileIterator_t* stealMappingOneToAllButSelf (ocrScheduler_t* base, u64 workerId ) {

    ocrSchedulerXp_t* derived = (ocrSchedulerXp_t*)base;
    u64 idx = (workerId - derived->workerIdFirst); // we always start workerIdFirst = 0 so we dont this this part

    xpThreadWorkpileIterator_t * stealIterator = &(derived->stealIterators[idx]);
    xpThreadWorkpileIteratorReset(stealIterator);

//printf("stealing on  %lu  for %lu \n",idx,workerId);
    return stealIterator;
}

void xpSchedulerDestruct(ocrScheduler_t * self) {
    u64 i;
    // Destruct the workpiles

    u64 count = self->workpileCount;
    for(i = 0; i < count; ++i) {
        self->workpiles[i]->fcts.destruct(self->workpiles[i]);
    }
    runtimeChunkFree((u64)(self->workpiles), NULL);

    runtimeChunkFree((u64)self, NULL);
}


void xpSchedulerBegin(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
    // Nothing to do locally
    u64 workpileCount = self->workpileCount;
    u64 i;

    for(i = 0; i < workpileCount; ++i) {
        self->workpiles[i]->fcts.begin(self->workpiles[i], PD);
    }
}

void xpSchedulerStart(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {

     // Get a GUID
    guidify(PD, (u64)self, &(self->fguid), OCR_GUID_SCHEDULER);
    self->pd = PD;
    ocrSchedulerXp_t * derived = (ocrSchedulerXp_t *) self;

    u64 threadsPerCore = derived->threadsPerCore;

    u64 workpileCount = self->workpileCount;
    u64 numIter = workpileCount * threadsPerCore;

//printf("workpile count: %lu \n", workpileCount);
//printf("threadsPerCore: %lu \n", threadsPerCore);

    u64 i;
    for(i = 0; i < workpileCount; ++i) {
        self->workpiles[i]->fcts.start(self->workpiles[i], PD);
    }

    ocrWorkpile_t ** workpiles = self->workpiles;

    // allocate steal iterator cache. Use pdMalloc since this is something
    // local to the policy domain and that will never be shared
    xpThreadWorkpileIterator_t * stealIteratorsCache = PD->fcts.pdMalloc(
        PD, sizeof(xpThreadWorkpileIterator_t)*numIter);

    // Initialize thread iterator cache
    i = 0;
    while(i < numIter) {
        // Note: here we assume workpile 'i' will match worker 'i' => Not great

        xpInitWorkpileIterator(&(stealIteratorsCache[i]),i/threadsPerCore ,workpileCount, workpiles);
        ++i;
    }

    derived->stealIterators = stealIteratorsCache;
}

void xpSchedulerStop(ocrScheduler_t * self) {
    ocrPolicyDomain_t *pd = NULL;
    ocrPolicyMsg_t msg;
    getCurrentEnv(&pd, NULL, NULL, &msg);

    // Stop the workpiles
    u64 i = 0;
    u64 count = self->workpileCount;
    for(i = 0; i < count; ++i) {
        self->workpiles[i]->fcts.stop(self->workpiles[i]);
    }

    // We need to destroy the stealIterators now because pdFree does not
    // exist after stop
    ocrSchedulerXp_t * derived = (ocrSchedulerXp_t *) self;
    pd->fcts.pdFree(pd, derived->stealIterators);

    // Destroy the GUID

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid) = self->fguid;
    PD_MSG_FIELD(guid.metaDataPtr) = self;
    PD_MSG_FIELD(properties) = 0;
    // Ignore failure, probably shutting down
    pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    self->fguid.guid = UNINITIALIZED_GUID;
}

void xpSchedulerFinish(ocrScheduler_t *self) {
    u64 i = 0;
    u64 count = self->workpileCount;
    for(i = 0; i < count; ++i) {
        self->workpiles[i]->fcts.finish(self->workpiles[i]);
    }
    // Nothing to do locally
}

u8 xpSchedulerTake (ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts) {
    // Source must be a worker guid and we rely on indices to map
    // workpile to worker (threadsPerCore-to-one)
    // TODO: This is a non-portable assumption but will do for now.
    ocrWorker_t *worker = NULL;
    ocrWorkerHc_t *hcWorker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    hcWorker = (ocrWorkerHc_t*)worker;

    if(*count == 0) return 1; // No room to put anything

    ASSERT(edts != NULL); // Array should be allocated at least

    u64 workerId = hcWorker->id;
    // First try to pop
    ocrWorkpile_t * wpToPop = popMappingOneToOne(self, workerId);
    // TODO sagnak, just to get it to compile, I am trickling down the 'cost' though it most probably is not the same
    // TODO: Add cost again

    ocrFatGuid_t popped = wpToPop->fcts.pop(wpToPop, POP_WORKPOPTYPE, NULL);
    if(NULL_GUID == popped.guid) {
        // If popping failed, try to steal
        xpThreadWorkpileIterator_t * it = stealMappingOneToAllButSelf(self, workerId);

           while(xpThreadWorkpileIteratorHasNext(it) && (NULL_GUID == popped.guid)) {
            ocrWorkpile_t * next = xpThreadWorkpileIteratorNext(it);
            popped = next->fcts.pop(next, STEAL_WORKPOPTYPE, NULL);
        }

    }

    // In this implementation we expect the caller to have
    // allocated memory for us since we can return at most one
    // guid (most likely store using the address of a local)
    if(NULL_GUID != popped.guid) {
        *count = 1;
        edts[0] = popped;
        hcWorker->backoffUs =1;

    } else {

        struct timespec tm;
        tm.tv_sec = hcWorker->backoffUs / 1e6;
        tm.tv_nsec = hcWorker->backoffUs * 1e3;
        ASSERT(nanosleep(&tm, NULL) != -1);

        hcWorker->backoffUs += hcWorker->backoffUs;
        hcWorker->backoffUs = (hcWorker->backoffUs > MAX_STEAL_TIMEOUT) ?
            MAX_STEAL_TIMEOUT : hcWorker->backoffUs;

        *count = 0;
    }
    return 0;
}

u8 xpSchedulerGive (ocrScheduler_t* base, u32* count, ocrFatGuid_t* edts) {
    // Source must be a worker guid and we rely on indices to map

       // workpile to worker (threadsPerCore-to-one)
    // TODO: This is a non-portable assumption but will do for now.
    ocrWorker_t *worker = NULL;
    ocrWorkerHc_t *hcWorker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    hcWorker = (ocrWorkerHc_t*)worker;

    // Source must be a worker guid
    u64 workerId = hcWorker->id;
    ocrWorkpile_t * wpToPush = pushMappingOneToOne(base, workerId);
    u32 i = 0;
    for ( ; i < *count; ++i ) {
        wpToPush->fcts.push(wpToPush, PUSH_WORKPUSHTYPE, edts[i]);
        edts[i].guid = NULL_GUID;
    }
    *count = 0;
    return 0;
}

ocrScheduler_t* newSchedulerXp(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerXp_t* derived = (ocrSchedulerXp_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerXp_t), NULL);

    ocrScheduler_t* base = (ocrScheduler_t*)derived;
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->pd = NULL;
    base->workpiles = NULL;
    base->workpileCount = 0;
    base->fcts = factory->schedulerFcts;

    paramListSchedulerXpInst_t *mapper = (paramListSchedulerXpInst_t*)perInstance;
    derived->workerIdFirst = mapper->workerIdFirst;
    derived->coreCount = mapper->coreCount;
    derived->threadsPerCore = mapper->threadsPerCore;

    return base;
}

void destructSchedulerFactoryXp(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryXp(ocrParamList_t *perType) {
    ocrSchedulerFactoryXp_t* derived = (ocrSchedulerFactoryXp_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryXp_t), (void *)1);

    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) derived;
    base->instantiate = &newSchedulerXp;
    base->destruct = &destructSchedulerFactoryXp;
    base->schedulerFcts.begin = &xpSchedulerBegin;
    base->schedulerFcts.start = &xpSchedulerStart;
    base->schedulerFcts.stop = &xpSchedulerStop;
    base->schedulerFcts.finish = &xpSchedulerFinish;
    base->schedulerFcts.destruct = &xpSchedulerDestruct;
    base->schedulerFcts.takeEdt = &xpSchedulerTake;
    base->schedulerFcts.giveEdt = &xpSchedulerGive;
    return base;
}

#endif /* ENABLE_SCHEDULER_XP */
