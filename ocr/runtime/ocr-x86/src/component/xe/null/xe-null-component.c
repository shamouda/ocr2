/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_XE_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "component/xe/xe-component.h"

ocrComponent_t* newComponentXe(ocrComponentFactory_t * self, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
    return NULL;
}

u8 xeComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 xeComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 xeComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryXe(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryXe_t), (void *)1);

    base->instantiate = &newComponentXe;
    base->destruct = NULL; //&destructComponentFactoryXe;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), xeComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), xeComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), xeComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrParamListHint_t*, u32), xeComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrParamListHint_t*, u32), xeComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrParamListHint_t*, u32), xeComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), xeComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), xeComponentSetLocation);

    return base;
}

#endif /* ENABLE_COMPONENT_XE_NULL */

