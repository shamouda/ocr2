/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_HC_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "component/hc/hc-component.h"

ocrComponent_t* newComponentHc(ocrComponentFactory_t * self, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
    return NULL;
}

u8 hcComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 hcComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hcComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryHc(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryHc_t), (void *)1);

    base->instantiate = &newComponentHc;
    base->destruct = NULL; //&destructComponentFactoryHc;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), hcComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), hcComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), hcComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrParamListHint_t*, u32), hcComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrParamListHint_t*, u32), hcComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrParamListHint_t*, u32), hcComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), hcComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), hcComponentSetLocation);

    return base;
}

#endif /* ENABLE_COMPONENT_HC_NULL */

