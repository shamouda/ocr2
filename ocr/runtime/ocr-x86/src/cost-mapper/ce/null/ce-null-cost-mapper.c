/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_CE_NULL
#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "cost-mapper/ce/ce-cost-mapper.h"

ocrCostMapper_t* newCostMapperCeNull(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperCe_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    return base;
}

void destructCostMapperFactoryCeNull(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void ceNullCostMapperDestruct(ocrCostMapper_t *self) {
    ASSERT(0);
}

void ceNullCostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void ceNullCostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void ceNullCostMapperStop(ocrCostMapper_t *self) {
    ASSERT(0);
}

void ceNullCostMapperFinish(ocrCostMapper_t *self) {
    ASSERT(0);
}

u8 ceNullCostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    return OCR_ENOTSUP;
}

u8 ceNullCostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    return OCR_ENOTSUP;
}

u8 ceNullCostMapperUpdate(ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties) {
    return OCR_ENOTSUP;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryCe(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryCe_t), (void *)1);

    base->instantiate = &newCostMapperCeNull;
    base->destruct = &destructCostMapperFactoryCeNull;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ceNullCostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), ceNullCostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceNullCostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceNullCostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), ceNullCostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, ocrFatGuid_t*, ocrFatGuid_t*, u32), ceNullCostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), ceNullCostMapperGive);
    base->fcts.update = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrFatGuid_t, ocrFatGuid_t, u32), ceNullCostMapperUpdate);

    return base;
}

#endif /* ENABLE_COST_MAPPER_CE_NULL */
