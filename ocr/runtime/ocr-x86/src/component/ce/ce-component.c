/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_BASE

#include "component/ce/ce-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"

ocrComponent_t* newComponentCeBase(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentCeBase_t * component = (ocrComponentCeBase_t *)runtimeChunkAlloc(sizeof(ocrComponentCeBase_t), (void *)1);
    component->base.fcts = factory->fcts;
    component->deque = newNonConcurrentQueue(pd, (void *) NULL_GUID);
    return (ocrComponent_t*)component;
}

u8 ceBaseComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceBaseComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentCeBase_t * ceBaseComp = (ocrComponentCeBase_t*)self;
    ceBaseComp->deque->pushAtTail(ceBaseComp->deque, (void*)component.guid, 0);
    return 0;
}

u8 ceBaseComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCeBase_t * ceBaseComp = (ocrComponentCeBase_t*)self;
    //ocrGuid_t el = (ocrGuid_t)ceBaseComp->deque->popFromTail(ceBaseComp->deque, 0);
    ocrGuid_t el = (ocrGuid_t)ceBaseComp->deque->popFromHead(ceBaseComp->deque, 1);
    if (el != NULL_GUID)
        component->guid = el;
    return 0;
}

u8 ceBaseComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceBaseComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceBaseComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 ceBaseComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceBaseComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
    self->mapping = loc;
    return 0;
}

ocrComponentFactory_t* newOcrComponentFactoryCe(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryCeBase_t), (void *)1);

    base->instantiate = &newComponentCeBase;
    base->destruct = NULL; //&destructComponentFactoryCeBase;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceBaseComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), ceBaseComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceBaseComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), ceBaseComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), ceBaseComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), ceBaseComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), ceBaseComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), ceBaseComponentSetLocation);

    return base;
}

#endif /* ENABLE_COMPONENT_CE_BASE */

