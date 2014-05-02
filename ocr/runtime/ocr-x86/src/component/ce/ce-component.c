/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#include "component/ce/ce-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"

#ifdef ENABLE_COMPONENT_CE_STATE

ocrComponent_t* newComponentCeState(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    //ocrComponentFactoryCeState_t * derivedFactory = (ocrComponentFactoryCeState_t*)factory;
    //u32 dequeInitSize = derivedFactory->dequeInitSize;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentCeState_t * component = (ocrComponentCeState_t *)runtimeChunkAlloc(sizeof(ocrComponentCeState_t), (void *)1);
    component->base.fcts = factory->fcts;
    component->base.mapping = (ocrLocation_t)hints.guid;
    component->deque = newNonConcurrentQueue(pd, (void *) NULL_GUID);
    return (ocrComponent_t*)component;
}

u8 ceStateComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceStateComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentCeState_t * ceStateComp = (ocrComponentCeState_t*)self;
    ceStateComp->deque->pushAtTail(ceStateComp->deque, (void*)component.guid, 0);
    return 0;
}

u8 ceStateComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCeState_t * ceStateComp = (ocrComponentCeState_t*)self;
    //ocrGuid_t el = (ocrGuid_t)ceStateComp->deque->popFromTail(ceStateComp->deque, 0);
    ocrGuid_t el = (ocrGuid_t)ceStateComp->deque->popFromHead(ceStateComp->deque, 1);
    if (el != NULL_GUID)
        component->guid = el;
    return 0;
}

u8 ceStateComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceStateComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceStateComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 ceStateComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryCeState(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryCeState_t), (void *)1);

    base->instantiate = &newComponentCeState;
    base->destruct = NULL; //&destructComponentFactoryCeState;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceStateComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), ceStateComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceStateComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), ceStateComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), ceStateComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), ceStateComponentMerge);
    base->fcts.count = FUNC_ADDR(u32 (*)(ocrComponent_t*, ocrLocation_t), ceStateComponentCount);

    paramListComponentFactCeState_t * paramDerived = (paramListComponentFactCeState_t*)perType;
    ocrComponentFactoryCeState_t * derived = (ocrComponentFactoryCeState_t*)base;
    derived->dequeInitSize = paramDerived->dequeInitSize;
    return base;
}

#endif /* ENABLE_COMPONENT_CE_STATE */

