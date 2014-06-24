/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_XE_NULL
#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "cost-mapper/xe/xe-cost-mapper.h"

ocrCostMapper_t* newCostMapperXeNull(ocrCostMapperFactory_t * factory, ocrParamList_t *perInstance) {
    ocrCostMapper_t* base = (ocrCostMapper_t*) runtimeChunkAlloc(sizeof(ocrCostMapperXe_t), NULL);
    base->fguid.guid = UNINITIALIZED_GUID;
    base->fguid.metaDataPtr = base;
    base->fcts = factory->fcts;
    return base;
}

void destructCostMapperFactoryXeNull(ocrCostMapperFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void xeNullCostMapperDestruct(ocrCostMapper_t *self) {
    ASSERT(0);
}

void xeNullCostMapperBegin(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void xeNullCostMapperStart(ocrCostMapper_t *self, ocrPolicyDomain_t * PD) {
    ASSERT(0);
}

void xeNullCostMapperStop(ocrCostMapper_t *self) {
    ASSERT(0);
}

void xeNullCostMapperFinish(ocrCostMapper_t *self) {
    ASSERT(0);
}

u8 xeNullCostMapperTake(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties) {
    return OCR_ENOTSUP;
}

u8 xeNullCostMapperGive(ocrCostMapper_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    return OCR_ENOTSUP;
}

u8 xeNullCostMapperUpdate(ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties) {
    return OCR_ENOTSUP;
}

ocrCostMapperFactory_t * newOcrCostMapperFactoryXe(ocrParamList_t *perType) {
    ocrCostMapperFactory_t* base = (ocrCostMapperFactory_t*) runtimeChunkAlloc(
        sizeof(ocrCostMapperFactoryXe_t), (void *)1);

    base->instantiate = &newCostMapperXeNull;
    base->destruct = &destructCostMapperFactoryXeNull;

    base->fcts.begin = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), xeNullCostMapperBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrCostMapper_t*, ocrPolicyDomain_t*), xeNullCostMapperStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrCostMapper_t*), xeNullCostMapperStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrCostMapper_t*), xeNullCostMapperFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrCostMapper_t*), xeNullCostMapperDestruct);
    base->fcts.take = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, ocrFatGuid_t*, ocrFatGuid_t*, u32), xeNullCostMapperTake);
    base->fcts.give = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), xeNullCostMapperGive);
    base->fcts.update = FUNC_ADDR(u8 (*)(ocrCostMapper_t*, ocrFatGuid_t, ocrFatGuid_t, u32), xeNullCostMapperUpdate);

    return base;
}

#endif /* ENABLE_COST_MAPPER_XE_NULL */
