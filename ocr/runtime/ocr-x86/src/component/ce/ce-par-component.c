/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_PAR

#include "component/ce/ce-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-tuning.h"
#include "ocr-sysboot.h"

ocrComponent_t* newComponentCePar(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    ocrComponentFactoryCePar_t * derivedFactory = (ocrComponentFactoryCePar_t*)factory;
    u32 maxWorkers = derivedFactory->maxWorkers;
    u32 maxGroups = derivedFactory->maxGroups;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentCePar_t * component = (ocrComponentCePar_t *)runtimeChunkAlloc(sizeof(ocrComponentCePar_t), (void *)1);
    component->base.fcts = factory->fcts;
    component->base.count = 0;
    component->affinities = (ocrComponent_t**)runtimeChunkAlloc(maxGroups * sizeof(ocrComponent_t*), (void *)1);
    component->workpiles = (ocrComponent_t**)runtimeChunkAlloc(maxWorkers * sizeof(ocrComponent_t*), (void *)1);
    component->numWorkers = maxWorkers;
    component->numGroups = maxGroups;
    u32 i;
    ocrComponentFactory_t * priorityFact = pd->componentFactories[componentPriority_id];
    for (i = 0; i < maxGroups; i++)
        component->affinities[i] = (ocrComponent_t*)priorityFact->instantiate(priorityFact, hints, 0);
    for (i = 0; i < maxWorkers; i++)
        component->workpiles[i] = NULL;
    return (ocrComponent_t*)component;
}

u8 ceParComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceParComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrTask_t * task = (ocrTask_t*)component.metaDataPtr;
    u64 affinity = (u64)task->affinity; //TODO: This should be passed through hints
    u64 hint = (u64)task->hints;
    ASSERT(affinity < ceParComp->numGroups);
    ocrComponent_t * ceAffinityComp = ceParComp->affinities[affinity];
    hints.guid = (ocrGuid_t)ocrHintGetPriority(hint);
    ceAffinityComp->fcts.insert(ceAffinityComp, loc, component, hints, properties);
    return 0;
}

u8 ceParComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrComponent_t * ceAffinityComp = ceParComp->workpiles[loc];
    if (ceAffinityComp) {
        ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
        if (component->guid == NULL_GUID) {
            ASSERT(ceAffinityComp->fcts.count(ceAffinityComp, loc) == 0);
            ceAffinityComp = NULL;
            ceParComp->workpiles[loc] = NULL;
        }
    }

    u32 idx = loc;
    if (ceAffinityComp == NULL) {
        while(idx < ceParComp->numGroups && ceParComp->affinities[idx]) {
            if (ceParComp->affinities[idx]->fcts.count(ceParComp->affinities[idx], loc) > 0) {
                ceAffinityComp = ceParComp->affinities[idx];
                ceParComp->workpiles[loc] = ceAffinityComp;
                break;
            }
            idx += ceParComp->numWorkers;
        }
        if (ceAffinityComp == NULL) {
            u32 i;
            for (i = 0; i < ceParComp->numWorkers; i++) {
                if (ceParComp->workpiles[i] != NULL) {
                    ceAffinityComp = ceParComp->workpiles[i];
                    ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
                    if (component->guid != NULL_GUID)
                        return 0;
                }
            }
        } else {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
        }
    }

    return 0;
}

u8 ceParComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceParComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ceParComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u64 ceParComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
return self->count;
}

u8 ceParComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryCe(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryCePar_t), (void *)1);

    base->instantiate = &newComponentCePar;
    base->destruct = NULL; //&destructComponentFactoryCePar;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceParComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), ceParComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), ceParComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), ceParComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), ceParComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), ceParComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), ceParComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), ceParComponentSetLocation);

    paramListComponentFactCePar_t * paramDerived = (paramListComponentFactCePar_t*)perType;
    ocrComponentFactoryCePar_t * derived = (ocrComponentFactoryCePar_t*)base;
    derived->maxWorkers = paramDerived->maxWorkers;
    derived->maxGroups = paramDerived->maxGroups;
    return base;
}

#endif /* ENABLE_COMPONENT_CE_PAR */

