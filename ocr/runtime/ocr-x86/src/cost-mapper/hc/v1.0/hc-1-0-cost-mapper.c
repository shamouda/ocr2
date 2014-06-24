/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_HC_1_0

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-scheduler.h"
#include "cost-mapper/hc/hc-cost-mapper.h"
#include "hint/hint-1-0/hint-1-0.h"
#include "component/hc/hc-component.h"

ocrCostMapper_t* newCostMapperHc_1_0(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperHc_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    return base;
}

void destructCostMapperFactoryHc_1_0(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void hc_1_0_CostMapperDestruct(ocrCostMapper_t *self) {
}

void hc_1_0_CostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
}

void hc_1_0_CostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    u32 i;
    ocrCostMapperHc_t * costMapper = (ocrCostMapperHc_t*)self;
    ocrComponentFactory_t* compFact = (ocrComponentFactory_t*)PD->componentFactories[0];
    costMapper->componentFactory = compFact;
    ASSERT(PD->neighborCount);
    costMapper->numWorkers = PD->neighborCount;
    costMapper->workerComponents = (ocrComponent_t**)runtimeChunkAlloc(PD->neighborCount * sizeof(ocrComponent_t*), NULL);
    for (i = 0; i < PD->neighborCount; i++) {
        costMapper->workerComponents[i] = compFact->instantiate(compFact, NULL, 0);
    }
}

void hc_1_0_CostMapperStop(ocrCostMapper_t *self) {
}

void hc_1_0_CostMapperFinish(ocrCostMapper_t *self) {
}

u8 hc_1_0_CostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    u32 i;
    ocrCostMapperHc_t * costMapper = (ocrCostMapperHc_t*)self;
    ASSERT((u64)source < costMapper->numWorkers);
    ocrComponent_t * comp = costMapper->workerComponents[source];
    ocrFatGuid_t fcomponent = {NULL_GUID, NULL};
    costMapper->componentFactory->fcts.remove(comp, source, &fcomponent, kind, hints, HC_COMP_PROP_POP);
    if (fcomponent.guid == NULL_GUID) {
        for (i = 1; i < costMapper->numWorkers; i++) {
            u32 victim = ((u64)source+i)%costMapper->numWorkers;
            comp = costMapper->workerComponents[victim];
            costMapper->componentFactory->fcts.remove(comp, source, &fcomponent, kind, hints, HC_COMP_PROP_STEAL);
            if (fcomponent.guid != NULL_GUID)
                break;
        }
    }
    if (fcomponent.guid != NULL_GUID)
        *component = fcomponent.guid;
    return 0;
}

u8 hc_1_0_CostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ocrCostMapperHc_t * costMapper = (ocrCostMapperHc_t*)self;
    ASSERT((u64)source < costMapper->numWorkers);
    ocrComponent_t * comp = costMapper->workerComponents[source];
    ocrFatGuid_t fcomponent = {component, NULL};
    RESULT_ASSERT(costMapper->componentFactory->fcts.insert(comp, source, fcomponent, kind, hints, properties), ==, 0);
    return 0;
}

u8 hc_1_0_CostMapperUpdate(ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties) {
    return OCR_ENOTSUP;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryHc(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryHc_t), (void *)1);

    base->instantiate = &newCostMapperHc_1_0;
    base->destruct = &destructCostMapperFactoryHc_1_0;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), hc_1_0_CostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), hc_1_0_CostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), hc_1_0_CostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), hc_1_0_CostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), hc_1_0_CostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, ocrFatGuid_t*, ocrFatGuid_t*, u32), hc_1_0_CostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_CostMapperGive);
    base->fcts.update = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hc_1_0_CostMapperUpdate);

    return base;
}

#endif /* ENABLE_COST_MAPPER_HC_1_0 */
