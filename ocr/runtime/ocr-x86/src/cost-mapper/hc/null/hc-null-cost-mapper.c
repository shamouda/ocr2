/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_HC_NULL
#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "cost-mapper/hc/hc-cost-mapper.h"

ocrCostMapper_t* newCostMapperHcNull(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperHc_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    return base;
}

void destructCostMapperFactoryHcNull(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void hcNullCostMapperDestruct(ocrCostMapper_t *self) {
    ASSERT(0);
}

void hcNullCostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void hcNullCostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void hcNullCostMapperStop(ocrCostMapper_t *self) {
    ASSERT(0);
}

void hcNullCostMapperFinish(ocrCostMapper_t *self) {
    ASSERT(0);
}

u8 hcNullCostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    return OCR_ENOTSUP;
}

u8 hcNullCostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    return OCR_ENOTSUP;
}

u8 hcNullCostMapperUpdate(ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties) {
    return OCR_ENOTSUP;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryHc(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryHc_t), (void *)1);

    base->instantiate = &newCostMapperHcNull;
    base->destruct = &destructCostMapperFactoryHcNull;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), hcNullCostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), hcNullCostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), hcNullCostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), hcNullCostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), hcNullCostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, ocrFatGuid_t*, ocrFatGuid_t*, u32), hcNullCostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), hcNullCostMapperGive);
    base->fcts.update = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcNullCostMapperUpdate);

    return base;
}

#endif /* ENABLE_COST_MAPPER_HC_NULL */
