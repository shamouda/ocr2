/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_HC_1_0

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "component/hc/hc-component.h"

ocrComponent_t* newComponentHc_1_0(ocrComponentFactory_t * self, ocrParamListHint_t *hints, u32 properties) {
    // Get the current environment
    ocrPolicyDomain_t *pd = NULL;
    ocrPolicyMsg_t msg;

    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
    msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = NULL_GUID;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    // We allocate everything in the meta-data to keep things simple
    PD_MSG_FIELD(size) = sizeof(ocrComponentHc_t);
    PD_MSG_FIELD(kind) = OCR_GUID_COMPONENT;
    PD_MSG_FIELD(properties) = 0;
    RESULT_PROPAGATE2(pd->fcts.processMessage(pd, &msg, true), NULL);
    ocrComponentHc_t *comp = (ocrComponentHc_t*)PD_MSG_FIELD(guid.metaDataPtr);
    ocrComponent_t *base = (ocrComponent_t*)comp;
    ASSERT(comp);

    // Set-up structures
    base->guid = PD_MSG_FIELD(guid.guid);
    base->state = COMPONENT_STATE_ALLOCATED;
    base->mapping = INVALID_LOCATION;
    base->count = 0;
    comp->deque = newWorkStealingDeque(pd, (void *) NULL_GUID);
#undef PD_MSG
#undef PD_TYPE
    return base;
}

u8 hc_1_0_ComponentCreate(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hc_1_0_ComponentInsert(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ocrComponentHc_t *comp = (ocrComponentHc_t*)self;
    ASSERT(component.guid != NULL_GUID);
    comp->deque->pushAtTail(comp->deque, (void *)(component.guid), 0);
    return 0;
}

u8 hc_1_0_ComponentRemove(ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ocrComponentHc_t *comp = (ocrComponentHc_t*)self;
    switch (properties) {
    case HC_COMP_PROP_POP:
        component->guid = (ocrGuid_t)comp->deque->popFromTail(comp->deque, 0);
        break;
    case HC_COMP_PROP_STEAL:
        component->guid = (ocrGuid_t)comp->deque->popFromHead(comp->deque, 1);
        break;
    default:
        ASSERT(0);
    }
    return 0;
}

u8 hc_1_0_ComponentMove(ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hc_1_0_ComponentSplit(ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hc_1_0_ComponentMerge(ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrParamListHint_t *hints, u32 properties) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u32 hc_1_0_ComponentCount(ocrComponent_t *self, ocrLocation_t loc) {
//TODO
ASSERT(0);
return OCR_ENOTSUP;
}

u8 hc_1_0_ComponentSetLocation(ocrComponent_t *self, ocrLocation_t loc) {
    self->mapping = loc;
    return 0;
}

ocrComponentFactory_t* newOcrComponentFactoryHc(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryHc_t), (void *)1);

    base->instantiate = &newComponentHc_1_0;
    base->destruct = NULL; //&destructComponentFactoryHc;

    base->fcts.create = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_ComponentCreate);
    base->fcts.insert = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_ComponentInsert);
    base->fcts.remove = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrFatGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_ComponentRemove);
    base->fcts.move = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, ocrComponent_t*, ocrFatGuid_t, ocrParamListHint_t*, u32), hc_1_0_ComponentMove);
    base->fcts.split = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, u32*, ocrFatGuid_t*, ocrParamListHint_t*, u32), hc_1_0_ComponentSplit);
    base->fcts.merge = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t, u32, ocrFatGuid_t*, ocrParamListHint_t*, u32), hc_1_0_ComponentMerge);
    base->fcts.count = FUNC_ADDR(u64 (*)(ocrComponent_t*, ocrLocation_t), hc_1_0_ComponentCount);
    base->fcts.setLocation = FUNC_ADDR(u8 (*)(ocrComponent_t*, ocrLocation_t), hc_1_0_ComponentSetLocation);

    return base;
}

#endif /* ENABLE_COMPONENT_HC_1_0 */

