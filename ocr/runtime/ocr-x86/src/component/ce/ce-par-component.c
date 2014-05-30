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
    component->base.mapping = NULL_LOCATION;
    u32 i;
    ocrComponentFactory_t * priorityFact = pd->componentFactories[componentPriority_id];

    ocrFatGuid_t affinityHints = {NULL_GUID, NULL};
    component->affinities = (ocrComponent_t**)runtimeChunkAlloc(maxGroups * sizeof(ocrComponent_t*), (void *)1);
    component->numGroups = maxGroups;
    for (i = 0; i < maxGroups; i++) {
        affinityHints.guid = (ocrGuid_t)i;
        component->affinities[i] = (ocrComponent_t*)priorityFact->instantiate(priorityFact, affinityHints, 0);
    }

    component->active = (ocrComponent_t**)runtimeChunkAlloc(maxWorkers * sizeof(ocrComponent_t*), (void *)1);
    component->numWorkers = maxWorkers;
    for (i = 0; i < maxWorkers; i++) {
        component->active[i] = NULL;
    }

#ifdef MULTIDEQUES
    component->affinityGroups = (deque_t**)runtimeChunkAlloc(maxWorkers * sizeof(deque_t*), (void *)1);
    for (i = 0; i < maxWorkers; i++) {
        component->affinityGroups[i] = newNonConcurrentQueue(pd, (void *) NULL_GUID);
    }
#else
    component->affinityGroups = newNonConcurrentQueue(pd, (void *) NULL_GUID);
#endif
    return (ocrComponent_t*)component;
}

u8 ceParComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

#if 0
u8 ceParComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrComponent_t * ceAffinityComp = ceParComp->affinities[0];
    ceAffinityComp->fcts.insert(ceAffinityComp, loc, component, hints, properties);
    return 0;
}

u8 ceParComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrTask_t * task = (ocrTask_t*)component.metaDataPtr;
    u64 affinity = (u64)task->affinity; //FIXME: This should be passed through hints
    ASSERT(affinity < ceParComp->numGroups);
    ocrComponent_t * ceAffinityComp = ceParComp->affinities[affinity];
    if (ceAffinityComp->mapping == NULL_LOCATION && ceAffinityComp->fcts.count(ceAffinityComp, loc) == 0) {
        listNode_t * node = ceParComp->affinityGroups->head;
        if (node == NULL) {
            ceParComp->affinityGroups->pushAtHead(ceParComp->affinityGroups, ceAffinityComp, 0);
        } else {
            //FIXME: linear insertion
            while(node) {
                ocrComponentPriority_t * comp = (ocrComponentPriority_t*)(node->data);
                if ((u64)(comp->affinity) >= affinity) {
                    ceParComp->affinityGroups->insertBefore(ceParComp->affinityGroups, node, ceAffinityComp, 0);
                    break;
                }
                node = node->next;
            }
            if (node == NULL)
                ceParComp->affinityGroups->pushAtTail(ceParComp->affinityGroups, ceAffinityComp, 0);
        }
    }
    ceAffinityComp->fcts.insert(ceAffinityComp, loc, component, hints, properties);
    return 0;
}
#else
u8 ceParComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    ASSERT((properties & OCR_COMP_PROP_TYPE) == OCR_COMP_PROP_TYPE_EDT);
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrTask_t * task = (ocrTask_t*)component.metaDataPtr;
    u64 affinity = (u64)task->affinity; //FIXME: This should be passed through hints
    ASSERT(affinity < ceParComp->numGroups);
    ocrComponent_t * ceAffinityComp = ceParComp->affinities[affinity];
    ceAffinityComp->fcts.insert(ceAffinityComp, loc, component, hints, properties);
    if (((ocrComponentPriority_t*)ceAffinityComp)->state == AFFINITY_COMPONENT_STATE_NEW) {
#ifdef MULTIDEQUES
        u32 mapping = affinity % ceParComp->numWorkers;
        deque_t * deq = ceParComp->affinityGroups[mapping];
#else
        deque_t * deq = ceParComp->affinityGroups;
#endif
        deq->pushAtTail(deq, ceAffinityComp, 0);
        ((ocrComponentPriority_t*)ceAffinityComp)->state = AFFINITY_COMPONENT_STATE_STAGED;
    }
    return 0;
}
#endif

#if 0
u8 ceParComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrComponent_t * ceAffinityComp = ceParComp->affinities[0];
    ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
    return 0;
}

u8 ceParComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrComponent_t * ceAffinityComp = ceParComp->active[loc];
    if (ceAffinityComp) {
        if (ceAffinityComp->fcts.count(ceAffinityComp, loc) == 0) {
            ceParComp->active[loc] = NULL;
            ceAffinityComp->mapping = NULL_LOCATION;
        } else {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
            return 0;
        }
    }

    if (ceParComp->affinityGroups->size > 0) {
        ceAffinityComp = ceParComp->affinityGroups->popFromHead(ceParComp->affinityGroups, 0);
        ASSERT(ceAffinityComp->fcts.count(ceAffinityComp, loc) > 0);
        ceParComp->active[loc] = ceAffinityComp;
        ceAffinityComp->mapping = loc;
        ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
        ASSERT(component->guid != NULL_GUID);
        return 0;
    }

    u32 i;
    for (i = 1; i < ceParComp->numWorkers; i++) {
        u32 victim = (loc+i) % ceParComp->numWorkers;
        ceAffinityComp = ceParComp->active[victim];
        if (ceAffinityComp && ceAffinityComp->fcts.count(ceAffinityComp, loc) > 0) {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
            return 0;
        }
    }

    return 0;
}
#else
u8 ceParComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    ocrComponentCePar_t * ceParComp = (ocrComponentCePar_t*)self;
    ocrComponent_t * ceAffinityComp = ceParComp->active[loc];
    if (ceAffinityComp) {
        if (ceAffinityComp->fcts.count(ceAffinityComp, loc) == 0) {
            ceParComp->active[loc] = NULL;
            ceAffinityComp->mapping = NULL_LOCATION;
            ((ocrComponentPriority_t*)ceAffinityComp)->state = AFFINITY_COMPONENT_STATE_NEW;
        } else {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
            return 0;
        }
    }

#ifdef MULTIDEQUES
    deque_t * deq = ceParComp->affinityGroups[loc];
#else
    deque_t * deq = ceParComp->affinityGroups;
#endif
    ceAffinityComp = deq->popFromHead(deq, 0);
    if (ceAffinityComp) {
        ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
        ASSERT(component->guid != NULL_GUID);
        ceParComp->active[loc] = ceAffinityComp;
        ceAffinityComp->mapping = loc;
        ((ocrComponentPriority_t*)ceAffinityComp)->state = AFFINITY_COMPONENT_STATE_ACTIVE;
        return 0;
    }

    u32 i;
#ifdef MULTIDEQUES
    for (i = 1; i < ceParComp->numWorkers; i++) {
        u32 idx = (loc+i) % ceParComp->numWorkers;
        deque_t * deq = ceParComp->affinityGroups[idx];
        ceAffinityComp = deq->popFromTail(deq, 0);
        if (ceAffinityComp) {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
            ceParComp->active[loc] = ceAffinityComp;
            ceAffinityComp->mapping = loc;
            ((ocrComponentPriority_t*)ceAffinityComp)->state = AFFINITY_COMPONENT_STATE_ACTIVE;
            return 0;
        }
    }
#endif

    for (i = 1; i < ceParComp->numWorkers; i++) {
        u32 victim = (loc+i) % ceParComp->numWorkers;
        ceAffinityComp = ceParComp->active[victim];
        if (ceAffinityComp && ceAffinityComp->fcts.count(ceAffinityComp, loc) > 0) {
            ceAffinityComp->fcts.remove(ceAffinityComp, loc, component, hints, properties);
            ASSERT(component->guid != NULL_GUID);
            return 0;
        }
    }

    return 0;
}
#endif

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

