/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler-heuristic/scheduler-heuristic-all.h"
#include "debug.h"

const char * schedulerHeuristic_types[] = {
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC
    "HC",
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_PC
    "PC",
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE
    "CE",
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL
    "NULL",
#endif
    NULL
};

ocrSchedulerHeuristicFactory_t * newSchedulerHeuristicFactory(schedulerHeuristicType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_HEURISTIC_HC
    case schedulerHeuristicHc_id:
        return newOcrSchedulerHeuristicFactoryHc(perType, type);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_PC
    case schedulerHeuristicPc_id:
        return newOcrSchedulerHeuristicFactoryPc(perType, type);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_CE
    case schedulerHeuristicCe_id:
        return newOcrSchedulerHeuristicFactoryCe(perType, type);
#endif
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL
    case schedulerHeuristicNull_id:
        return newOcrSchedulerHeuristicFactoryNull(perType, type);
#endif
    default:
        break;
    }
    ASSERT(0);
    return NULL;
}

void initializeSchedulerHeuristicOcr(ocrSchedulerHeuristicFactory_t * factory, ocrSchedulerHeuristic_t * self, ocrParamList_t *perInstance) {
    paramListSchedulerHeuristic_t *params = (paramListSchedulerHeuristic_t*)perInstance;
    self->fguid.guid = UNINITIALIZED_GUID;
    self->fguid.metaDataPtr = self;
    self->scheduler = NULL;
    self->contexts = NULL;
    self->contextCount = 0;
    self->costTable = NULL;
    self->fcts = factory->fcts;
    self->isMaster = params->isMaster;
    self->factoryId = factory->factoryId;
}
