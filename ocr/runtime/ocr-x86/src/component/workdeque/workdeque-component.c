/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_WORKDEQUE

#include "component/workdeque/workdeque-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"

ocrComponent_t* newComponentWorkdeque(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    //ocrComponentFactoryWorkdeque_t * derivedFactory = (ocrComponentFactoryWorkdeque_t*)factory;
    //u32 dequeInitSize = derivedFactory->dequeInitSize;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentWorkdeque_t * component = (ocrComponentWorkdeque_t *)runtimeChunkAlloc(sizeof(ocrComponentWorkdeque_t), (void *)1);
    component->base.fcts = factory->fcts;
    component->base.mapping = 0;
    component->deque = newWorkStealingDeque(pd, (void *) NULL_GUID);
    return (ocrComponent_t*)component;
}

u8 workdequeComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 workdequeComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentWorkdeque_t * workdequeComp = (ocrComponentWorkdeque_t*)self;
    ASSERT(loc == workdequeComp->base.mapping);
    workdequeComp->deque->pushAtTail(workdequeComp->deque, (void*)component.guid, 0);
    return 0;
}

u8 workdequeComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentWorkdeque_t * workdequeComp = (ocrComponentWorkdeque_t*)self;
    ocrGuid_t el;
    if (loc == workdequeComp->base.mapping) {
        el = (ocrGuid_t)workdequeComp->deque->popFromTail(workdequeComp->deque, 0);
    } else {
        el = (ocrGuid_t)workdequeComp->deque->popFromHead(workdequeComp->deque, 1);
    }
    if (el != NULL_GUID)
        component->guid = el;
    return 0;
}

u8 workdequeComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 workdequeComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 workdequeComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u64 workdequeComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 workdequeComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
    self->mapping = loc;
    return 0;
}

ocrComponentFactory_t* newOcrComponentFactoryWorkdeque(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryWorkdeque_t), (void *)1);

    base->instantiate = &newComponentWorkdeque;
    base->destruct = NULL; //&destructComponentFactoryWorkdeque;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), workdequeComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), workdequeComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), workdequeComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), workdequeComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), workdequeComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), workdequeComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), workdequeComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), workdequeComponentSetLocation);

    paramListComponentFactWorkdeque_t * paramDerived = (paramListComponentFactWorkdeque_t*)perType;
    ocrComponentFactoryWorkdeque_t * derived = (ocrComponentFactoryWorkdeque_t*)base;
    derived->dequeInitSize = paramDerived->dequeInitSize;
    return base;
}

#endif /* ENABLE_COMPONENT_WORKDEQUE */
