/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_PRIORITY

#include "component/priority/priority-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-tuning.h"
#include "ocr-sysboot.h"

ocrComponent_t* newComponentPriority(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    ocrComponentFactoryPriority_t * derivedFactory = (ocrComponentFactoryPriority_t*)factory;
    u32 maxLevels = derivedFactory->maxLevels;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentPriority_t * component = (ocrComponentPriority_t *)runtimeChunkAlloc(sizeof(ocrComponentPriority_t), (void *)1);
    component->base.fcts = factory->fcts;
    component->base.count = 0;
    component->base.mapping = NULL_LOCATION;
    component->workpiles = (deque_t**)runtimeChunkAlloc(maxLevels * sizeof(deque_t*), (void *)1);
    component->numLevels = maxLevels;
    component->affinity = hints.guid;
    component->startLevel = maxLevels;
    component->state = AFFINITY_COMPONENT_STATE_NEW;
    u32 i;
    for (i = 0; i < maxLevels; i++)
        component->workpiles[i] = newNonConcurrentQueue(pd, (void *) NULL_GUID);
    return (ocrComponent_t*)component;
}

u8 priorityComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 priorityComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentPriority_t * priorityComp = (ocrComponentPriority_t*)self;
    u64 level = (u64)ocrHintGetPriority(hints.guid);
    ASSERT(level < priorityComp->numLevels);
    deque_t * deq = priorityComp->workpiles[level];
    deq->pushAtTail(deq, (void*)component.guid, 0);
    self->count++;
    if (level < priorityComp->startLevel)
        priorityComp->startLevel = level;
    return 0;
}

u8 priorityComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    u32 i;
    ocrComponentPriority_t * priorityComp = (ocrComponentPriority_t*)self;
    for (i = priorityComp->startLevel; i < priorityComp->numLevels; i++) {
        deque_t * deq = priorityComp->workpiles[i];
        //ocrGuid_t el = (ocrGuid_t)deq->popFromTail(deq, 0);
        ocrGuid_t el = (ocrGuid_t)deq->popFromHead(deq, 0);
        if (el != NULL_GUID) {
            component->guid = el;
            self->count--;
            priorityComp->startLevel = i;
            break;
        }
    }
    return 0;
}


u8 priorityComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 priorityComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 priorityComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u64 priorityComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
return self->count;
}

u8 priorityComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryPriority(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryPriority_t), (void *)1);

    base->instantiate = &newComponentPriority;
    base->destruct = NULL; //&destructComponentFactoryPriority;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), priorityComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), priorityComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), priorityComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), priorityComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), priorityComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), priorityComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), priorityComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), priorityComponentSetLocation);

    paramListComponentFactPriority_t * paramDerived = (paramListComponentFactPriority_t*)perType;
    ocrComponentFactoryPriority_t * derived = (ocrComponentFactoryPriority_t*)base;
    derived->maxLevels = paramDerived->maxLevels;
    return base;
}

#endif /* ENABLE_COMPONENT_PRIORITY */

