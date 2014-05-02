/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_CE

#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-component.h"
//#include "component/ce/ce-component.h"
#include "component/component-all.h"
#include "cost-mapper/cost-mapper-all.h"
#include "ce-cost-mapper.h"

ocrCostMapper_t* newCostMapperCe(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperCe_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    base->componentState = NULL;
    return base;
}

void destructCostMapperFactoryCe(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void ceCostMapperDestruct(ocrCostMapper_t *self) {
}

void ceCostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
}

void ceCostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ocrCostMapperCe_t * ceCostMapper = (ocrCostMapperCe_t*)self;
    ocrComponentFactory_t * ceCompStateFact = PD->componentFactories[0];
    ocrFatGuid_t hints = {NULL_GUID, NULL};
    ceCostMapper->base.componentState = ceCompStateFact->instantiate(ceCompStateFact, hints, 0);
}

void ceCostMapperStop(ocrCostMapper_t *self) {
}

void ceCostMapperFinish(ocrCostMapper_t *self) {
}

u8 ceCostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrFatGuid_t *component, ocrFatGuid_t hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    ocrComponent_t *state = self->componentState;
    state->fcts.remove(state, source, component, hints, properties);
    return 0;
}

u8 ceCostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ocrComponent_t *state = self->componentState;
    state->fcts.insert(state, source, component, hints, properties);
    return 0;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryCe(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryCe_t), (void *)1);

    base->instantiate = &newCostMapperCe;
    base->destruct = &destructCostMapperFactoryCe;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ceCostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ceCostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceCostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceCostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceCostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, ocrFatGuid_t*, ocrFatGuid_t*, u32), ceCostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), ceCostMapperGive);

    return base;
}

#endif /* ENABLE_COST_MAPPER_CE */
