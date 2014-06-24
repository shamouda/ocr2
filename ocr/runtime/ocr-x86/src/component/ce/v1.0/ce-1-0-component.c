/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_1_0

#include "component/ce/ce-component.h"
#include "component/component-all.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "hint/hint-1-0/hint-1-0.h"

ocrComponentFactoryCe_1_0_t *gComponentFactoryCe_1_0 = NULL;

ocrComponent_t* newComponentCe_1_0(ocrComponentFactory_t * self, ocrParamListHint_t *hints, u32 properties) {
    ocrComponentFactoryCe_1_0_t * factory = (ocrComponentFactoryCe_1_0_t*)self;
    ocrComponentCe_1_0_t * compCe = NULL;

    if (factory->pFreeComponentHead == NULL) {
        if (factory->componentBlockIndex < COMPONENT_ALLOC_SIZE) {
            ocrFatGuid_t *fguid = &(factory->componentBlock[factory->componentBlockIndex++]);
            compCe = (ocrComponentCe_1_0_t*)fguid->metaDataPtr;
            compCe->base.guid = fguid->guid;
        } else {
            ASSERT(factory->componentBlock == NULL); //FIXME: until we get a destructor list
            ocrPolicyDomain_t *pd = NULL;
            ocrPolicyMsg_t msg;
            getCurrentEnv(&pd, NULL, NULL, &msg);
            ocrFatGuid_t *fguid = NULL;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_ARRAY_CREATE
            msg.type = PD_MSG_GUID_ARRAY_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
            PD_MSG_FIELD(guid) = &fguid;
            PD_MSG_FIELD(size) = sizeof(ocrComponentCe_1_0_t);
            PD_MSG_FIELD(count) = COMPONENT_ALLOC_SIZE;
            PD_MSG_FIELD(kind) = OCR_GUID_COMPONENT;
            PD_MSG_FIELD(properties) = 0;
            pd->fcts.processMessage (pd, &msg, true);
#undef PD_MSG
#undef PD_TYPE
            factory->componentBlock = fguid;
            factory->componentBlockIndex = 1;
            compCe = (ocrComponentCe_1_0_t*)fguid->metaDataPtr;
            compCe->base.guid = fguid->guid;
        }
    } else {
        compCe = factory->pFreeComponentHead;
        factory->pFreeComponentHead = compCe->next;
        compCe->next = NULL;
    }

    ocrComponent_t * component = (ocrComponent_t*)compCe;
    component->state = COMPONENT_STATE_ALLOCATED;
    //component->mapping = INVALID_LOCATION;
    component->count = 0;
    compCe->pElementHead = NULL;
    compCe->pComponentHead = NULL;
    compCe->next = NULL;
    return component;
}

static inline ocrComponentEl_1_0_t* newComponentElementCe_1_0() {
    ocrComponentEl_1_0_t *el = NULL;
    if (gComponentFactoryCe_1_0->pFreeElementHead != NULL) {
        el = gComponentFactoryCe_1_0->pFreeElementHead;
        gComponentFactoryCe_1_0->pFreeElementHead = el->next;
    } else {
        if(gComponentFactoryCe_1_0->elementBlockIndex < COMPONENT_ELEMENT_ALLOC_SIZE) {
            el = &(gComponentFactoryCe_1_0->elementBlock[gComponentFactoryCe_1_0->elementBlockIndex++]);
        } else {
            //FIXME: until we get a destructor list
            gComponentFactoryCe_1_0->elementBlock = (ocrComponentEl_1_0_t*)runtimeChunkAlloc(COMPONENT_ELEMENT_ALLOC_SIZE * sizeof(ocrComponentEl_1_0_t), (void*)1);
            el = gComponentFactoryCe_1_0->elementBlock;
            gComponentFactoryCe_1_0->elementBlockIndex = 1;
        }
    }
    return el;
}

u8 ce_1_0_ComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ASSERT(kind == OCR_GUID_COMPONENT);
    ocrComponent_t *comp = newComponentCe_1_0((ocrComponentFactory_t*)gComponentFactoryCe_1_0, NULL, 0);
    ocrComponentCe_1_0_t *compCe_1_0 = (ocrComponentCe_1_0_t*)comp;
    ocrComponentCe_1_0_t *parentComp = (ocrComponentCe_1_0_t*)self;
    compCe_1_0->next = parentComp->pComponentHead;
    parentComp->pComponentHead = compCe_1_0;
    component->guid = comp->guid;
    component->metaDataPtr = comp;
    return 0;
}

u8 ce_1_0_ComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ocrComponentCe_1_0_t *parentComp = (ocrComponentCe_1_0_t*)self;
    if (kind == OCR_GUID_EDT) {
        ocrComponentEl_1_0_t *el = newComponentElementCe_1_0();
        el->data = component.guid;
        el->next = parentComp->pElementHead;
        parentComp->pElementHead = el;
    } else {
        ASSERT(kind == OCR_GUID_COMPONENT);
        ASSERT(component.metaDataPtr != NULL);
        ocrComponentCe_1_0_t *comp = (ocrComponentCe_1_0_t*)(component.metaDataPtr);
        comp->next = parentComp->pComponentHead;
        parentComp->pComponentHead = comp;
    }
    self->count++;
    return 0;
}

u8 ce_1_0_ComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ASSERT(kind == OCR_GUID_EDT);
    ASSERT(component->guid == NULL_GUID);
    ocrComponentCe_1_0_t *parentComp = (ocrComponentCe_1_0_t*)self;
    if (parentComp->pComponentHead != NULL) {
        ocrComponent_t * compHead = (ocrComponent_t*)parentComp->pComponentHead;
        ce_1_0_ComponentRemove(compHead, loc, component, kind, hints, properties);
        ASSERT(component->guid != NULL_GUID);
        if (compHead->count == 0) {
            ocrComponentCe_1_0_t *compHead_1_0 = parentComp->pComponentHead;
            parentComp->pComponentHead = compHead_1_0->next;
            //compHead_1_0->next = NULL; //TODO: Handle prescribed and free ones
        }
    } else {
        ASSERT(parentComp->pElementHead != NULL);
        ocrComponentEl_1_0_t *el = parentComp->pElementHead;
        parentComp->pElementHead = el->next;
        component->guid = el->data;
        el->next = gComponentFactoryCe_1_0->pFreeElementHead;
        gComponentFactoryCe_1_0->pFreeElementHead = el;
        ocrParamListHint_1_0_t *compHint = (ocrParamListHint_1_0_t*)hints;
        compHint->component.guid = self->guid;
        compHint->component.metaDataPtr = self;
    }
    self->count--;
    return 0;
}

u8 ce_1_0_ComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ce_1_0_ComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ce_1_0_ComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 ce_1_0_ComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 ce_1_0_ComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

ocrComponentFactory_t* newOcrComponentFactoryCe(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryCe_1_0_t), (void *)1);

    base->instantiate = &newComponentCe_1_0;
    base->destruct = NULL; //&destructComponentFactoryCe_1_0;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), ce_1_0_ComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), ce_1_0_ComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), ce_1_0_ComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrParamListHint_t*, u32), ce_1_0_ComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrParamListHint_t*, u32), ce_1_0_ComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrParamListHint_t*, u32), ce_1_0_ComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), ce_1_0_ComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), ce_1_0_ComponentSetLocation);

    ocrComponentFactoryCe_1_0_t *derived = (ocrComponentFactoryCe_1_0_t*)base;
    derived->componentBlock = NULL;
    derived->componentBlockIndex = COMPONENT_ALLOC_SIZE;
    derived->pFreeComponentHead = NULL;
    derived->elementBlock = (ocrComponentEl_1_0_t*)runtimeChunkAlloc(COMPONENT_ELEMENT_ALLOC_SIZE * sizeof(ocrComponentEl_1_0_t), (void*)1);
    derived->elementBlockIndex = 0;
    derived->pFreeElementHead = NULL;

    gComponentFactoryCe_1_0 = derived;
    return base;
}

#endif /* ENABLE_COMPONENT_CE_1_0 */

