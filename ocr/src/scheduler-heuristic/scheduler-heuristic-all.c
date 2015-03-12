/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler-heuristic/scheduler-heuristic-all.h"
#include "debug.h"

const char * schedulerHeuristic_types[] = {
    "HC",
    "HC_COMM",
    "XE",
    "CE",
    "NULL",
    NULL
};

ocrSchedulerHeuristicFactory_t * newSchedulerHeuristicFactory(schedulerHeuristicType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC
    case schedulerHeuristicHc_id:
    {
#ifdef ENABLE_SCHEDULER_OBJECT_WST
        return newOcrSchedulerHeuristicFactoryHc(perType);
#endif
        break;
    }
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC_COMM
    case schedulerHeuristicHcComm_id:
        return newOcrSchedulerHeuristicFactoryHcComm(perType);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE
    case schedulerHeuristicCe_id:
        return newOcrSchedulerHeuristicFactoryCe(perType);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_XE
    case schedulerHeuristicXe_id:
        return newOcrSchedulerHeuristicFactoryXe(perType);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL
    case schedulerHeuristicNull_id:
        return newOcrSchedulerHeuristicFactoryNull(perType);
#endif
    default:
        break;
    }
    ASSERT(0);
    return NULL;
}

void initializeSchedulerHeuristicOcr(ocrSchedulerHeuristicFactory_t * factory, ocrSchedulerHeuristic_t * self, ocrParamList_t *perInstance) {
    self->fguid.guid = UNINITIALIZED_GUID;
    self->fguid.metaDataPtr = self;
    self->scheduler = NULL;
    self->contexts = NULL;
    self->contextCount = 0;
    self->costTable = NULL;
    self->fcts = factory->fcts;
}
