/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HC

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "scheduler/hc/hc-scheduler.h"
#include "scheduler/hc/scheduler-blocking-support.h"
#include "scheduler-heuristic/hc/hc-scheduler-heuristic.h"

/******************************************************/
/* Support structures                                 */
/******************************************************/
static inline void workpileIteratorReset(hcWorkpileIterator_t *base) {
    base->curr = ((base->id) + 1) % base->mod;
}

static inline bool workpileIteratorHasNext(hcWorkpileIterator_t * base) {
    return base->id != base->curr;
}

static inline ocrWorkpile_t * workpileIteratorNext(hcWorkpileIterator_t * base) {
    u64 current = base->curr;
    ocrWorkpile_t * toBeReturned = base->workpiles[current];
    base->curr = (current+1) % base->mod;
    return toBeReturned;
}

static inline void initWorkpileIterator(hcWorkpileIterator_t *base, u64 id,
                                        u64 workpileCount, ocrWorkpile_t ** workpiles ) {

    base->workpiles = workpiles;
    base->id = id;
    base->mod = workpileCount;
    // The 'curr' field is initialized by reset
    workpileIteratorReset(base);
}

/******************************************************/
/* OCR-HC SCHEDULER                                   */
/******************************************************/

static inline ocrWorkpile_t * popMappingOneToOne (ocrScheduler_t* base, u64 workerId ) {
    u64 idx = (workerId - ((ocrSchedulerHc_t*)base)->workerIdFirst);
    return base->workpiles[idx];
}

static inline ocrWorkpile_t * pushMappingOneToOne (ocrScheduler_t* base, u64 workerId ) {
    u64 idx = (workerId - ((ocrSchedulerHc_t*)base)->workerIdFirst);
    return base->workpiles[idx];
}

static inline hcWorkpileIterator_t* stealMappingOneToAllButSelf (ocrScheduler_t* base, u64 workerId ) {
    u64 idx = (workerId - ((ocrSchedulerHc_t*)base)->workerIdFirst);
    ocrSchedulerHc_t* derived = (ocrSchedulerHc_t*) base;
    hcWorkpileIterator_t * stealIterator = &(derived->stealIterators[idx]);
    workpileIteratorReset(stealIterator);
    return stealIterator;
}

void hcSchedulerDestruct(ocrScheduler_t * self) {
    u64 i;
    // Destruct the workpiles
    u64 count = self->workpileCount;
    for(i = 0; i < count; ++i) {
        self->workpiles[i]->fcts.destruct(self->workpiles[i]);
    }

    self->rootObj->fcts.destruct(self->rootObj);

    //scheduler heuristics
    u64 schedulerHeuristicCount = self->schedulerHeuristicCount;
    for(i = 0; i < schedulerHeuristicCount; ++i) {
        self->schedulerHeuristics[i]->fcts.destruct(self->schedulerHeuristics[i]);
    }

    runtimeChunkFree((u64)(self->workpiles), PERSISTENT_CHUNK);
    runtimeChunkFree((u64)(self->schedulerHeuristics), PERSISTENT_CHUNK);
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 hcSchedulerSwitchRunlevel(ocrScheduler_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                             phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    u64 i;
    if(runlevel == RL_CONFIG_PARSE && (properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
        // First transition, setup some backpointers
        self->rootObj->scheduler = self;
        for(i = 0; i < self->schedulerHeuristicCount; ++i) {
            self->schedulerHeuristics[i]->scheduler = self;
        }
    }

    if(properties & RL_BRING_UP) {
        // Take care of all other sub-objects
        for(i = 0; i < self->workpileCount; ++i) {
            toReturn |= self->workpiles[i]->fcts.switchRunlevel(
                self->workpiles[i], PD, runlevel, phase, properties, NULL, 0);
        }
        toReturn |= self->rootObj->fcts.switchRunlevel(self->rootObj, PD, runlevel, phase,
                                                       properties, NULL, 0);
        for(i = 0; i < self->schedulerHeuristicCount; ++i) {
            toReturn |= self->schedulerHeuristics[i]->fcts.switchRunlevel(
                self->schedulerHeuristics[i], PD, runlevel, phase, properties, NULL, 0);
        }
    }

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        if((properties & RL_BRING_UP) && phase == 0) {
            RL_ENSURE_PHASE_UP(PD, RL_MEMORY_OK, RL_PHASE_SCHEDULER, 2);
            RL_ENSURE_PHASE_DOWN(PD, RL_MEMORY_OK, RL_PHASE_SCHEDULER, 2);
        }
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
            self->pd = PD;
            self->contextCount = self->pd->workerCount;
        }
        break;
    case RL_MEMORY_OK:
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
            // allocate steal iterator cache. Use pdMalloc since this is something
            // local to the policy domain and that will never be shared
            hcWorkpileIterator_t * stealIteratorsCache = self->pd->fcts.pdMalloc(
                self->pd, sizeof(hcWorkpileIterator_t)*self->workpileCount);

            // Initialize steal iterator cache
            for(i = 0; i < self->workpileCount; ++i) {
                // Note: here we assume workpile 'i' will match worker 'i' => Not great
                initWorkpileIterator(&(stealIteratorsCache[i]), i, self->workpileCount,
                                     self->workpiles);
            }
            ocrSchedulerHc_t * derived = (ocrSchedulerHc_t *) self;
            derived->stealIterators = stealIteratorsCache;
        }

        if((properties & RL_TEAR_DOWN) && RL_IS_FIRST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
            ocrSchedulerHc_t *derived = (ocrSchedulerHc_t*)self;
            self->pd->fcts.pdFree(self->pd, derived->stealIterators);
        }
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_ALLOCATOR);
            }
        } else {
            // Tear-down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_COMPUTE_OK, phase)) {
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
                msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
                PD_MSG_FIELD_I(guid) = self->fguid;
                PD_MSG_FIELD_I(properties) = 0;
                toReturn |= self->pd->fcts.processMessage(self->pd, &msg, false);
                self->fguid.guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
            }
        }
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }

    if(properties & RL_TEAR_DOWN) {
        // Take care of all other sub-objects
        for(i = 0; i < self->workpileCount; ++i) {
            toReturn |= self->workpiles[i]->fcts.switchRunlevel(
                self->workpiles[i], PD, runlevel, phase, properties, NULL, 0);
        }
        toReturn |= self->rootObj->fcts.switchRunlevel(self->rootObj, PD, runlevel, phase,
                                                       properties, NULL, 0);
        for(i = 0; i < self->schedulerHeuristicCount; ++i) {
            toReturn |= self->schedulerHeuristics[i]->fcts.switchRunlevel(
                self->schedulerHeuristics[i], PD, runlevel, phase, properties, NULL, 0);
        }
    }
    return toReturn;
}

u8 hcSchedulerTakeEdt (ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts) {
    // Source must be a worker guid and we rely on indices to map
    // workers to workpiles (one-to-one)
    // BUG #586: This is a non-portable assumption but will do for now.

    if(*count == 0) return 1; // No room to put anything

    ocrWorker_t *worker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    ocrFatGuid_t popped;
    u64 workerId;

    ASSERT(edts != NULL); // Array should be allocated at least

    {
        START_PROFILE(sched_hc_Pop);
        workerId = worker->seqId;
        // First try to pop
        ocrWorkpile_t * wpToPop = popMappingOneToOne(self, workerId);
        popped = wpToPop->fcts.pop(wpToPop, POP_WORKPOPTYPE, NULL);
        EXIT_PROFILE;
    }

    START_PROFILE(sched_hc_Steal);
    if(NULL_GUID == popped.guid) {
        // If popping failed, try to steal
        hcWorkpileIterator_t* it = stealMappingOneToAllButSelf(self, workerId);
        while(workpileIteratorHasNext(it) && (NULL_GUID == popped.guid)) {
            ocrWorkpile_t * next = workpileIteratorNext(it);
            popped = next->fcts.pop(next, STEAL_WORKPOPTYPE, NULL);
        }
    }
    EXIT_PROFILE;

    // In this implementation we expect the caller to have
    // allocated memory for us since we can return at most one
    // guid (most likely store using the address of a local)
    if(NULL_GUID != popped.guid) {
        *count = 1;
        edts[0] = popped;
    } else {
        *count = 0;
    }
    return 0;
}

u8 hcSchedulerGiveEdt (ocrScheduler_t* base, u32* count, ocrFatGuid_t* edts) {
    // Source must be a worker guid and we rely on indices to map
    // workers to workpiles (one-to-one)
    // BUG #586: This is a non-portable assumption but will do for now.
    ocrWorker_t *worker = NULL;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    // Source must be a worker guid
    ocrWorkpile_t * wpToPush = pushMappingOneToOne(base, worker->seqId);
    u32 i = 0;
    for ( ; i < *count; ++i ) {
        if (((ocrTask_t *)edts[i].metaDataPtr)->state == ALLACQ_EDTSTATE) {
            wpToPush->fcts.push(wpToPush, PUSH_WORKPUSHTYPE, edts[i]);
            edts[i].guid = NULL_GUID;
        }
    }
    *count = 0;
    return 0;
}

u8 hcSchedulerTakeComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return 0;
}

u8 hcSchedulerGiveComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return 0;
}

u8 hcSchedulerMonitorProgress(ocrScheduler_t *self, ocrMonitorProgress_t type, void * monitoree) {
#ifdef ENABLE_SCHEDULER_BLOCKING_SUPPORT
    // Current implementation assumes the worker is blocked.
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    handleWorkerNotProgressing(worker);
#endif
    return 0;
}

///////////////////////////////
//      Scheduler 1.0        //
///////////////////////////////

u8 hcSchedulerRegisterContext(ocrScheduler_t *self, u64 contextId, ocrLocation_t loc) {
    u32 i;
    for (i = 0; i < self->schedulerHeuristicCount; i++) {
        self->schedulerHeuristics[i]->fcts.registerContext(self->schedulerHeuristics[i], contextId, loc);
    }
    return 0;
}

u8 hcSchedulerGive(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[0];
    ocrSchedulerHeuristicContext_t *context = schedulerHeuristic->fcts.getContext(schedulerHeuristic, opArgs->contextId);
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GIVE].invoke(schedulerHeuristic, context, opArgs, hints);
}

u8 hcSchedulerTake(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    if (opArgs->el == NULL) {
        paramListSchedulerObject_t compParams;
        compParams.kind = opArgs->takeKind;
        compParams.count = opArgs->takeCount;
        ocrSchedulerObject_t *sComp = (ocrSchedulerObject_t*)self->rootObj;
        ocrSchedulerObjectFactory_t *sFact = self->pd->schedulerObjectFactories[sComp->fctId];
        opArgs->el = sFact->fcts.create(sFact, (ocrParamList_t*)(&compParams));
        ASSERT(opArgs->el);
    }
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[0];
    ocrSchedulerHeuristicContext_t *context = schedulerHeuristic->fcts.getContext(schedulerHeuristic, opArgs->contextId);
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TAKE].invoke(schedulerHeuristic, context, opArgs, hints);
}

u8 hcSchedulerDone(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerObjectFactory_t *fact = self->pd->schedulerObjectFactories[opArgs->el->fctId];
    fact->fcts.destruct(fact, opArgs->el);
    opArgs->el = NULL;
    return 0;
}

u8 hcSchedulerDoneAndTake(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    bool done = true;
    if (opArgs->el->kind == opArgs->takeKind) {
        ASSERT(opArgs->el->kind == OCR_SCHEDULER_OBJECT_EDT); //This scheduler only support EDT schedulerObjects
        if (opArgs->takeCount == 1) done = false;
    }
    if (done) hcSchedulerDone(self, opArgs, hints);
    return hcSchedulerTake(self, opArgs, hints);
}

u8 hcSchedulerUpdate(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs) {
    return OCR_ENOTSUP;
}

ocrScheduler_t* newSchedulerHc(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* base = (ocrScheduler_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHc_t), PERSISTENT_CHUNK);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeSchedulerHc(ocrSchedulerFactory_t * factory, ocrScheduler_t *self, ocrParamList_t *perInstance) {
    initializeSchedulerOcr(factory, self, perInstance);
    ocrSchedulerHc_t* derived = (ocrSchedulerHc_t*) self;
    paramListSchedulerHcInst_t *mapper = (paramListSchedulerHcInst_t*)perInstance;
    derived->workerIdFirst = mapper->workerIdFirst;
}

void destructSchedulerFactoryHc(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryHc(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryHc_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newSchedulerHc;
    base->initialize  = &initializeSchedulerHc;
    base->destruct = &destructSchedulerFactoryHc;
    base->schedulerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                          phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), hcSchedulerSwitchRunlevel);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), hcSchedulerDestruct);
    base->schedulerFcts.takeEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), hcSchedulerTakeEdt);
    base->schedulerFcts.giveEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), hcSchedulerGiveEdt);
    base->schedulerFcts.takeComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), hcSchedulerTakeComm);
    base->schedulerFcts.giveComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), hcSchedulerGiveComm);
    base->schedulerFcts.monitorProgress = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrMonitorProgress_t, void*), hcSchedulerMonitorProgress);

    //Scheduler 1.0
    base->schedulerFcts.registerContext = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u64, ocrLocation_t), hcSchedulerRegisterContext);
    base->schedulerFcts.update = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*), hcSchedulerUpdate);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_GIVE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerGive);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_TAKE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerTake);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_DONE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerDone);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_DONE_TAKE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), hcSchedulerDoneAndTake);
    return base;
}

#endif /* ENABLE_SCHEDULER_HC */
