/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "component/ce/ce-component.h"

ocrComponent_t* newComponentCe(ocrComponentFactory_t * self, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
    return NULL;
}

u8 ceComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 ceComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryCe(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryCe_t), (void *)1);

    base->instantiate = &newComponentCe;
    base->destruct = NULL; //&destructComponentFactoryCe;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), ceComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), ceComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), ceComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrParamListHint_t*, u32), ceComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrParamListHint_t*, u32), ceComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrParamListHint_t*, u32), ceComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), ceComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), ceComponentSetLocation);

    return base;
}

#endif /* ENABLE_COMPONENT_CE_NULL */

