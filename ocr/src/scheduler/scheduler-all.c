/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler/scheduler-all.h"
#include "debug.h"

const char * scheduler_types[] = {
#ifdef ENABLE_SCHEDULER_COMMON
    "COMMON",
#endif
#ifdef ENABLE_SCHEDULER_HC
    "HC",
#endif
#ifdef ENABLE_SCHEDULER_HC_COMM_DELEGATE
    "HC_COMM_DELEGATE",
#endif
#ifdef ENABLE_SCHEDULER_XE
    "XE",
#endif
#ifdef ENABLE_SCHEDULER_CE
    "CE",
#endif
    NULL
};

ocrSchedulerFactory_t * newSchedulerFactory(schedulerType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_COMMON
    case schedulerCommon_id:
        return newOcrSchedulerFactoryCommon(perType);
#endif
#ifdef ENABLE_SCHEDULER_HC
    case schedulerHc_id:
        return newOcrSchedulerFactoryHc(perType);
#endif
#ifdef ENABLE_SCHEDULER_HC_COMM_DELEGATE
    case schedulerHcCommDelegate_id:
        return newOcrSchedulerFactoryHcCommDelegate(perType);
#endif
#ifdef ENABLE_SCHEDULER_XE
    case schedulerXe_id:
        return newOcrSchedulerFactoryXe(perType);
#endif
#ifdef ENABLE_SCHEDULER_CE
    case schedulerCe_id:
        return newOcrSchedulerFactoryCe(perType);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

void initializeSchedulerOcr(ocrSchedulerFactory_t * factory, ocrScheduler_t * self, ocrParamList_t *perInstance) {
    self->fguid.guid = UNINITIALIZED_GUID;
    self->fguid.metaDataPtr = self;
    self->pd = NULL;
    self->workpiles = NULL;
    self->workpileCount = 0;
    self->rootObj = NULL;
    self->schedulerHeuristics = NULL;
    self->schedulerHeuristicCount = 0;
    self->fcts = factory->schedulerFcts;
}
