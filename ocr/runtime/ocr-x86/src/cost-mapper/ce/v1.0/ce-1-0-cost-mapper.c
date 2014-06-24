/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_CE_1_0

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-scheduler.h"
#include "cost-mapper/cost-mapper-all.h"
#include "cost-mapper/ce/ce-cost-mapper.h"
#include "hint/hint-1-0/hint-1-0.h"

//#define CE_BASE

ocrCostMapper_t* newCostMapperCe_1_0(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperCe_1_0_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    return base;
}

void destructCostMapperFactoryCe_1_0(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void ce_1_0_CostMapperDestruct(ocrCostMapper_t *self) {
}

void ce_1_0_CostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
}

void ce_1_0_CostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    u32 i, l;
    ocrCostMapperCe_1_0_t * costMapper = (ocrCostMapperCe_1_0_t*)self;
    ocrComponentFactory_t* compFact = (ocrComponentFactory_t*)PD->componentFactories[0];
    costMapper->componentFactory = compFact;
    costMapper->sharedComponent = compFact->instantiate(compFact, NULL, 0);
    ASSERT(PD->neighborCount);
    costMapper->numWorkers = PD->neighborCount;
    for (l = 0; l < MAX_PRIORITY_LEVELS; l++) {
        costMapper->priorityLevels[l] = (ocrComponent_t**)runtimeChunkAlloc(PD->neighborCount * sizeof(ocrComponent_t*), NULL);
        for (i = 0; i < PD->neighborCount; i++) {
            costMapper->priorityLevels[l][i] = compFact->instantiate(compFact, NULL, 0);
            //costMapper->priorityLevels[l][i]->mapping = (ocrLocation_t)i;
        }
    }
    costMapper->priorityLevelCount = (u64*)runtimeChunkAlloc(MAX_PRIORITY_LEVELS * sizeof(u64), 0);
    for (i = 0; i < MAX_PRIORITY_LEVELS; i++) {
        costMapper->priorityLevelCount[i] = 0;
    }

    costMapper->priorityLevelWorker = (u32*)runtimeChunkAlloc(PD->neighborCount * sizeof(u32), 0);
    costMapper->workerAvailable = (bool*)runtimeChunkAlloc(PD->neighborCount * sizeof(bool), 0);
    costMapper->stealAttempts = (u32*)runtimeChunkAlloc(PD->neighborCount * sizeof(u32), 0);
    for (i = 0; i < PD->neighborCount; i++) {
        costMapper->priorityLevelWorker[i] = MAX_PRIORITY_LEVELS;
        costMapper->workerAvailable[i] = false;
        costMapper->stealAttempts[i] = 0;
    }
    costMapper->count = 0;
    costMapper->maxLoad = 0;
    costMapper->maxStealRepels = 0;//10 * (PD->neighborCount - 1);
}

void ce_1_0_CostMapperStop(ocrCostMapper_t *self) {
}

void ce_1_0_CostMapperFinish(ocrCostMapper_t *self) {
}

u8 ce_1_0_CostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    ocrCostMapperCe_1_0_t * costMapper = (ocrCostMapperCe_1_0_t*)self;
    if (costMapper->count == 0) return 0;
    ocrFatGuid_t fcomponent = {.guid = NULL_GUID, .metaDataPtr = NULL};
#ifdef CE_BASE
    RESULT_ASSERT(costMapper->componentFactory->fcts.remove(costMapper->sharedComponent, source, &fcomponent, kind, hints, properties), ==, 0);
    ASSERT(fcomponent.guid != NULL_GUID);
    *component = fcomponent.guid;
    costMapper->count--;
#else
    u32 i, j, priLevel;
    for (priLevel = costMapper->priorityLevelWorker[source]; priLevel < MAX_PRIORITY_LEVELS; priLevel++) {
        ocrComponent_t *mapperComponent = costMapper->priorityLevels[priLevel][source];
        if (mapperComponent->count > 0) {
            RESULT_ASSERT(costMapper->componentFactory->fcts.remove(mapperComponent, source, &fcomponent, kind, hints, properties), ==, 0);
            costMapper->priorityLevelCount[priLevel]--;
            break;
        }
    }
    if (priLevel == MAX_PRIORITY_LEVELS) { //no local elements found
        costMapper->workerAvailable[source] = false;
        //first check for shared elements
        if (costMapper->sharedComponent->count > 0) {
            RESULT_ASSERT(costMapper->componentFactory->fcts.remove(costMapper->sharedComponent, source, &fcomponent, kind, hints, properties), ==, 0);
        } else {//if no shared element found , steal from other workers
            for (i = 0; i < MAX_PRIORITY_LEVELS; i++) {
                if (costMapper->priorityLevelCount[i] != 0) {
                    for (j = 1; j < costMapper->numWorkers; j++) {
                        u32 victim = ((u64)source+j)%costMapper->numWorkers;
                        if (i >= costMapper->priorityLevelWorker[victim]) {
                            ocrComponent_t *victimComponent = costMapper->priorityLevels[i][victim];
                            if (victimComponent->count > 0) {
                                if (costMapper->stealAttempts[victim] >= costMapper->maxStealRepels) {
                                    RESULT_ASSERT(costMapper->componentFactory->fcts.remove(victimComponent, source, &fcomponent, kind, hints, properties), ==, 0);
                                    costMapper->priorityLevelCount[i]--;
                                    break;
                                } else {
                                    costMapper->stealAttempts[victim]++;
                                }
                            }
                        }
                    } //end for(j)
                    if (j < costMapper->numWorkers)
                        break;
                }
            } //end for(i)
        }
    }
    if (fcomponent.guid != NULL_GUID) {
        *component = fcomponent.guid;
        costMapper->priorityLevelWorker[source] = priLevel;
        costMapper->count--;
    }
#endif
    return 0;
}

u8 ce_1_0_CostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ocrCostMapperCe_1_0_t * costMapper = (ocrCostMapperCe_1_0_t*)self;
    ocrFatGuid_t fcomponent = {component, NULL};
#ifdef CE_BASE
    RESULT_ASSERT(costMapper->componentFactory->fcts.insert(costMapper->sharedComponent, source, fcomponent, kind, hints, properties), ==, 0);
    costMapper->count++;
#else
    if (hints) {
        ASSERT(hints->type == HINT_PARAM_SCHED_EDT);
        ocrParamListHint_1_0_t *hintParam = (ocrParamListHint_1_0_t*)hints;
        ocrComponent_t *affinityComponent = (ocrComponent_t*)(hintParam->component.metaDataPtr);
        if (affinityComponent != NULL && affinityComponent->count > 0) {
            RESULT_ASSERT(costMapper->componentFactory->fcts.insert(affinityComponent, source, fcomponent, kind, hints, properties), ==, 0);
        } else {
            u32 priLevel = PRIORITY_LEVEL(hintParam->priority);
            ocrLocation_t mapping = (hintParam->mapping == INVALID_LOCATION) ? source : hintParam->mapping;
            ASSERT(mapping < costMapper->numWorkers);
            ocrComponent_t *mapperComponent = costMapper->priorityLevels[priLevel][mapping];
            if (affinityComponent != NULL) {
                RESULT_ASSERT(costMapper->componentFactory->fcts.insert(affinityComponent, source, fcomponent, kind, hints, properties), ==, 0);
                fcomponent.metaDataPtr = affinityComponent;
                RESULT_ASSERT(costMapper->componentFactory->fcts.insert(mapperComponent, source, fcomponent, OCR_GUID_COMPONENT, hints, properties), ==, 0);
            } else {
                RESULT_ASSERT(costMapper->componentFactory->fcts.insert(mapperComponent, source, fcomponent, kind, hints, properties), ==, 0);
            }
            costMapper->priorityLevelCount[priLevel]++;
            if (costMapper->priorityLevelWorker[mapping] > priLevel)
                costMapper->priorityLevelWorker[mapping] = priLevel;
            if (costMapper->workerAvailable[mapping] == false) {
                costMapper->workerAvailable[mapping] = true;
                costMapper->stealAttempts[mapping] = 0;
            }
        }
    } else {
        RESULT_ASSERT(costMapper->componentFactory->fcts.insert(costMapper->sharedComponent, source, fcomponent, kind, hints, properties), ==, 0);
    }
    costMapper->count++;
    if (costMapper->count > costMapper->maxLoad)
        costMapper->maxLoad = costMapper->count;
    /*fprintf(stderr, "Max Count: %lu\n", (u64)costMapper->maxLoad);*/
#endif
    return 0;
}

u8 ce_1_0_CostMapperUpdate(ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties) {
    return OCR_ENOTSUP;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryCe(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryCe_1_0_t), (void *)1);

    base->instantiate = &newCostMapperCe_1_0;
    base->destruct = &destructCostMapperFactoryCe_1_0;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ce_1_0_CostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ce_1_0_CostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), ce_1_0_CostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), ce_1_0_CostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), ce_1_0_CostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, ocrFatGuid_t*, ocrFatGuid_t*, u32), ce_1_0_CostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), ce_1_0_CostMapperGive);
    base->fcts.update = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrFatGuid_t, ocrFatGuid_t, u32), ce_1_0_CostMapperUpdate);

    return base;
}

#endif /* ENABLE_COST_MAPPER_CE_1_0 */
