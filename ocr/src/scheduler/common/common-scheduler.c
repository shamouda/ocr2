/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_COMMON

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "scheduler/common/common-scheduler.h"
// BUG #793 Probably need to make this not HC-specific
#include "scheduler/hc/scheduler-blocking-support.h"

/******************************************************/
/* OCR-COMMON SCHEDULER                               */
/******************************************************/

void commonSchedulerDestruct(ocrScheduler_t * self) {
    u64 i;

    // Destruct the root scheduler object
    ocrSchedulerObjectRootFactory_t *rootFact = (ocrSchedulerObjectRootFactory_t*)self->pd->schedulerObjectFactories[self->rootObj->fctId];
    rootFact->fcts.destruct(self->rootObj);

    //scheduler heuristics
    u64 schedulerHeuristicCount = self->schedulerHeuristicCount;
    for(i = 0; i < schedulerHeuristicCount; ++i) {
        self->schedulerHeuristics[i]->fcts.destruct(self->schedulerHeuristics[i]);
    }

    runtimeChunkFree((u64)(self->schedulerHeuristics), PERSISTENT_CHUNK);
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 commonSchedulerSwitchRunlevel(ocrScheduler_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        bool masterFound = false;
        for(i = 0; i < self->schedulerHeuristicCount; ++i) {
            ocrSchedulerHeuristic_t *heuristic = self->schedulerHeuristics[i];
            heuristic->scheduler = self;
            if (heuristic->isMaster) {
                self->masterHeuristicId = i;
                ASSERT(!masterFound);
                masterFound = true;
            }
        }
        if (!masterFound) {
            //If master is not specified, choose the first heuristic to be master
            self->masterHeuristicId = 0;
            self->schedulerHeuristics[0]->isMaster = true;
        }
    }

    if(properties & RL_BRING_UP) {
        // Take care of all other sub-objects
        ocrSchedulerObjectRootFactory_t *rootFact = (ocrSchedulerObjectRootFactory_t*)PD->schedulerObjectFactories[self->rootObj->fctId];
        toReturn |= rootFact->fcts.switchRunlevel(self->rootObj, PD, runlevel, phase, properties, NULL, 0);
        // Do not re-order: Scheduler object root should be brought up before heuristics
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
        }
        break;
    case RL_MEMORY_OK:
        if((properties & RL_BRING_UP) && RL_IS_LAST_PHASE_UP(PD, RL_MEMORY_OK, phase)) {
        }

        if((properties & RL_TEAR_DOWN) && RL_IS_FIRST_PHASE_DOWN(PD, RL_MEMORY_OK, phase)) {
        }
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_COMPUTE_OK, phase)) {
                // We get a GUID for ourself
                guidify(self->pd, (u64)self, &(self->fguid), OCR_GUID_SCHEDULER);
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
        ocrSchedulerObjectRootFactory_t *rootFact = (ocrSchedulerObjectRootFactory_t*)PD->schedulerObjectFactories[self->rootObj->fctId];
        toReturn |= rootFact->fcts.switchRunlevel(self->rootObj, PD, runlevel, phase, properties, NULL, 0);

        for(i = 0; i < self->schedulerHeuristicCount; ++i) {
            toReturn |= self->schedulerHeuristics[i]->fcts.switchRunlevel(
                self->schedulerHeuristics[i], PD, runlevel, phase, properties, NULL, 0);
        }
    }
    return toReturn;
}

u8 commonSchedulerTakeEdt (ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts) {
    ASSERT(0); //old scheduler interfaces not supported
    return OCR_ENOTSUP;
}

u8 commonSchedulerGiveEdt (ocrScheduler_t* base, u32* count, ocrFatGuid_t* edts) {
    ASSERT(0); //old scheduler interfaces not supported
    return OCR_ENOTSUP;
}

u8 commonSchedulerTakeComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    ASSERT(0); //old scheduler interfaces not supported
    return OCR_ENOTSUP;
}

u8 commonSchedulerGiveComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    ASSERT(0); //old scheduler interfaces not supported
    return OCR_ENOTSUP;
}

// BUG #793 deprecate this api in favor of update
u8 commonSchedulerMonitorProgress(ocrScheduler_t *self, ocrMonitorProgress_t type, void * monitoree) {
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

u8 commonSchedulerGetWorkInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[self->masterHeuristicId];
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GET_WORK].invoke(schedulerHeuristic, opArgs, hints);
}

u8 commonSchedulerNotifyInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[self->masterHeuristicId];
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_NOTIFY].invoke(schedulerHeuristic, opArgs, hints);
}

u8 commonSchedulerTransactInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    u32 heuristicId = self->masterHeuristicId;
    u32 i;
    for(i = 0; i < self->schedulerHeuristicCount; ++i) {
        if (self->schedulerHeuristics[i]->factoryId == opArgs->heuristicId) {
            heuristicId = i;
            break;
        }
    }
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[heuristicId];
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TRANSACT].invoke(schedulerHeuristic, opArgs, hints);
}

u8 commonSchedulerAnalyzeInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    u32 heuristicId = self->masterHeuristicId;
    u32 i;
    for(i = 0; i < self->schedulerHeuristicCount; ++i) {
        if (self->schedulerHeuristics[i]->factoryId == opArgs->heuristicId) {
            heuristicId = i;
            break;
        }
    }
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[heuristicId];
    return schedulerHeuristic->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_ANALYZE].invoke(schedulerHeuristic, opArgs, hints);
}

u8 commonSchedulerUpdate(ocrScheduler_t *self, u32 properties) {
    ocrSchedulerHeuristic_t *schedulerHeuristic = self->schedulerHeuristics[self->masterHeuristicId];
    return schedulerHeuristic->fcts.update(schedulerHeuristic, properties);
}

ocrScheduler_t* newSchedulerCommon(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* base = (ocrScheduler_t*) runtimeChunkAlloc(sizeof(ocrSchedulerCommon_t), PERSISTENT_CHUNK);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeSchedulerCommon(ocrSchedulerFactory_t * factory, ocrScheduler_t *self, ocrParamList_t *perInstance) {
    initializeSchedulerOcr(factory, self, perInstance);
}

void destructSchedulerFactoryCommon(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryCommon(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryCommon_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newSchedulerCommon;
    base->initialize  = &initializeSchedulerCommon;
    base->destruct = &destructSchedulerFactoryCommon;
    base->schedulerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                          phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), commonSchedulerSwitchRunlevel);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), commonSchedulerDestruct);
    base->schedulerFcts.takeEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), commonSchedulerTakeEdt);
    base->schedulerFcts.giveEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), commonSchedulerGiveEdt);
    base->schedulerFcts.takeComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), commonSchedulerTakeComm);
    base->schedulerFcts.giveComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), commonSchedulerGiveComm);
    base->schedulerFcts.monitorProgress = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrMonitorProgress_t, void*), commonSchedulerMonitorProgress);

    //Scheduler 1.0
    base->schedulerFcts.update = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32), commonSchedulerUpdate);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), commonSchedulerGetWorkInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), commonSchedulerNotifyInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), commonSchedulerTransactInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), commonSchedulerAnalyzeInvoke);
    return base;
}

#endif /* ENABLE_SCHEDULER_COMMON */
