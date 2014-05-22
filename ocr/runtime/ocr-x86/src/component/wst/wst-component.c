/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_WST

#include "component/wst/wst-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"

ocrComponent_t* newComponentWst(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    u32 i;

    ocrComponentFactoryWst_t * derivedFactory = (ocrComponentFactoryWst_t*)factory;
    u32 numWorkers = derivedFactory->maxWorkers;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentWst_t * component = (ocrComponentWst_t *)runtimeChunkAlloc((sizeof(ocrComponentWst_t) + (numWorkers * sizeof(ocrComponent_t*))), (void *)1);
    component->base.fcts = factory->fcts;
    component->components = (ocrComponent_t **)((u64)component + sizeof(ocrComponentWst_t));
    component->numWorkers = numWorkers;
    ocrComponentFactory_t * fact = pd->componentFactories[componentWorkdeque_id];
    ocrFatGuid_t hint = {NULL_GUID, NULL};
    for (i = 0; i < numWorkers; i++) {
        ocrComponent_t * workComp = fact->instantiate(fact, hint, 0);
        workComp->fcts.setLocation(workComp, i);
        component->components[i] = workComp;
    }
    return (ocrComponent_t*)component;
}

u8 wstComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 wstComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT(component.guid != NULL_GUID);
    ocrComponentWst_t * compWst = (ocrComponentWst_t*)self;
    ASSERT((u32)loc < compWst->numWorkers);
    ocrComponent_t * comp = compWst->components[loc];
    comp->fcts.insert(comp, loc, component, hints, properties);
    return 0;
}

u8 wstComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    u32 i;
    ASSERT(component && component->guid == NULL_GUID);
    ocrComponentWst_t * compWst = (ocrComponentWst_t*)self;
    //First try to pop from owned deque and then try to steal from others
    ocrComponent_t * comp = compWst->components[loc];
    comp->fcts.remove(comp, loc, component, hints, properties);
    if (component->guid == NULL_GUID) {
        for (i = 1; i < compWst->numWorkers; i++) {
            u32 victim = ((u32)loc + i) % compWst->numWorkers;
            ocrComponent_t *compVictim = compWst->components[victim];
            compVictim->fcts.remove(compVictim, loc, component, hints, properties);
            if (component->guid != NULL_GUID) {
                break;
            }
        }
    }
    return 0;
}

u8 wstComponentMove(ocrComponent_t *self, ocrLocation_t loc, ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 wstComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 wstComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u64 wstComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 wstComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}


ocrComponentFactory_t* newOcrComponentFactoryWst(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryWst_t), (void *)1);

    base->instantiate = &newComponentWst;
    base->destruct = NULL; //&destructComponentFactoryWst;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), wstComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), wstComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), wstComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), wstComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), wstComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), wstComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), wstComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), wstComponentSetLocation);

    paramListComponentFactWst_t * paramDerived = (paramListComponentFactWst_t*)perType;
    ocrComponentFactoryWst_t * derived = (ocrComponentFactoryWst_t*)base;
    derived->maxWorkers = paramDerived->maxWorkers;
    ASSERT(derived->maxWorkers);
    return base;
}

#endif /* ENABLE_COMPONENT_WST */

