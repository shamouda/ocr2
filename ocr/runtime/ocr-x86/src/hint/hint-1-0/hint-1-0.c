/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_HINT_1_0

#define DEBUG_TYPE HINT_1_0
#include "debug.h"
#include "utils/ocr-utils.h"
#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-hint.h"
#include "hint/hint-1-0/hint-1-0.h"

static ocrHint_t* newHintEdtTemp_1_0(ocrHintFactory_t* fact, u32 properties) {
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
    PD_MSG_FIELD(size) = sizeof(ocrHintEdtTemp_t);
    PD_MSG_FIELD(kind) = OCR_GUID_HINT;
    PD_MSG_FIELD(properties) = 0;
    RESULT_PROPAGATE2(pd->fcts.processMessage(pd, &msg, true), NULL);
    ocrHintEdtTemp_t *edtHint = (ocrHintEdtTemp_t*)PD_MSG_FIELD(guid.metaDataPtr);
    ocrHint_t *hint = (ocrHint_t*)edtHint;
    ASSERT(edtHint);

    // Set-up base structures
    hint->guid = PD_MSG_FIELD(guid.guid);
    hint->type = OCR_HINT_EDT_TEMPLATE;

    edtHint->edtPriority = 0; //By default, max priority.
    edtHint->dominantDbSlot = (u32)-1;
    edtHint->dominantDbWeight = 0;
#undef PD_MSG
#undef PD_TYPE

    return hint;
}

ocrHint_t* newHint_1_0(ocrHintFactory_t* fact, ocrHintTypes_t type, u32 properties) {
    switch(type) {
    case OCR_HINT_EDT_TEMPLATE: return newHintEdtTemp_1_0(fact, properties);
    default: ASSERT(0); break;
    }
    return NULL;
}

u8 initializeHint_1_0(ocrHint_t* hint, ocrHintTypes_t type, u32 properties, ocrHint_t* otherHint) {
    switch(type) {
    case OCR_HINT_EDT: {
        ocrHintEdt_t* edtHint = (ocrHintEdt_t*)hint;
        ASSERT(otherHint && otherHint->type == OCR_HINT_EDT_TEMPLATE);
        ocrHintEdtTemp_t* edtTempHint = (ocrHintEdtTemp_t*)otherHint;
        edtHint->base.type = OCR_HINT_EDT;
        edtHint->priority = edtTempHint->edtPriority;
        edtHint->dominantDbSlot = edtTempHint->dominantDbSlot;
        edtHint->mapping = INVALID_LOCATION;
        edtHint->affinity = NULL_GUID;
        break;
        }
    case OCR_HINT_EDT_TEMPLATE: {
        ASSERT(otherHint == NULL);
        ocrHintEdtTemp_t* edtTempHint = (ocrHintEdtTemp_t*)hint;
        edtTempHint->base.type = OCR_HINT_EDT_TEMPLATE;
        edtTempHint->edtPriority = 0;
        edtTempHint->dominantDbSlot = (u32)(-1);
        edtTempHint->dominantDbWeight = 0;
        break;
        }
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 setHint_1_0(ocrHint_t* hint, ocrHint_t* otherHint) {
    switch(hint->type) {
    case OCR_HINT_EDT_TEMPLATE: {
        ocrHintEdtTemp_t* edtTempHint = (ocrHintEdtTemp_t*)hint;
        ocrHintEdtTemp_t* edtTempHintOther = (ocrHintEdtTemp_t*)otherHint;
        edtTempHint->edtPriority = edtTempHintOther->edtPriority;
        edtTempHint->dominantDbSlot = edtTempHintOther->dominantDbSlot;
        edtTempHint->dominantDbWeight = edtTempHintOther->dominantDbWeight;
        break;
        }
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

ocrHint_t * getHint_1_0(ocrHint_t* hint) {
    ASSERT(0);
    return NULL;
}

u32 sizeofHint_1_0(ocrHintTypes_t type, u32 properties) {
    switch(type) {
    case OCR_HINT_EDT:
        return (u32)sizeof(ocrHintEdt_t);
    case OCR_HINT_EDT_TEMPLATE:
        return (u32)sizeof(ocrHintEdtTemp_t);
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 setPriorityHint_1_0(ocrHint_t* hint, u32 priority) {
    switch(hint->type) {
    case OCR_HINT_EDT_TEMPLATE:
        ((ocrHintEdtTemp_t*)hint)->edtPriority = priority;
        break;
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 getPriorityHint_1_0(ocrHint_t* hint, u32* priority) {
    switch(hint->type) {
    case OCR_HINT_EDT_TEMPLATE:
        *priority = ((ocrHintEdtTemp_t*)hint)->edtPriority;
        break;
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 setAffinityHint_1_0(ocrHint_t* hint, ocrGuid_t affinity) {
    switch(hint->type) {
    case OCR_HINT_EDT:
        ((ocrHintEdt_t*)hint)->affinity = affinity;
        break;
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 getAffinityHint_1_0(ocrHint_t* hint, ocrGuid_t *affinity) {
    switch(hint->type) {
    case OCR_HINT_EDT:
        *affinity = ((ocrHintEdt_t*)hint)->affinity;
        break;
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 setDepWeightHint_1_0(ocrHint_t* hint, u32 slot, u32 weight) {
    switch(hint->type) {
    case OCR_HINT_EDT_TEMPLATE: {
        ocrHintEdtTemp_t* edtHint = (ocrHintEdtTemp_t*)hint;
        if (slot < 255 && edtHint->dominantDbWeight < weight) {
            edtHint->dominantDbSlot = slot;
            edtHint->dominantDbWeight = weight;
        }
        break;
        }
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 getDepWeightHint_1_0(ocrHint_t* hint, u32* slot, u32* weight) {
    switch(hint->type) {
    case OCR_HINT_EDT_TEMPLATE: {
        ASSERT(slot == NULL);
        *slot = ((ocrHintEdtTemp_t*)hint)->dominantDbSlot;
        *weight = ((ocrHintEdtTemp_t*)hint)->dominantDbWeight;
        break;
        }
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

u8 destructHint_1_0(ocrHint_t* hint) {
    runtimeChunkFree((u64)hint, NULL);
    return 0;
}

void destructHintFactory_1_0(ocrHintFactory_t* base) {
    runtimeChunkFree((u64)base, NULL);
}

ocrHintFactory_t * newHintFactory_1_0(ocrParamList_t* perInstance, u32 factoryId) {
    ocrHintFactory_t* base = (ocrHintFactory_t*)runtimeChunkAlloc(sizeof(ocrHintFactory_1_0_t), NULL);

    base->instantiate = FUNC_ADDR(ocrHint_t* (*) (ocrHintFactory_t*, ocrHintTypes_t, u32), newHint_1_0);
    base->destruct =  FUNC_ADDR(void (*) (ocrHintFactory_t*), destructHintFactory_1_0);
    base->factoryId = factoryId;
    base->fcts.destruct = FUNC_ADDR(u8 (*)(ocrHint_t*), destructHint_1_0);
    base->fcts.initialize = FUNC_ADDR(u8 (*)(ocrHint_t*, ocrHintTypes_t, u32, ocrHint_t*), initializeHint_1_0);
    base->fcts.setHint = FUNC_ADDR(u8 (*)(ocrHint_t*, ocrHint_t*), setHint_1_0);
    base->fcts.getHint = FUNC_ADDR(ocrHint_t* (*)(ocrHint_t*), getHint_1_0);
    base->fcts.sizeofHint = FUNC_ADDR(u32 (*)(ocrHintTypes_t, u32), sizeofHint_1_0);
    base->fcts.setPriority = FUNC_ADDR(u8 (*)(ocrHint_t*, u32), setPriorityHint_1_0);
    base->fcts.getPriority = FUNC_ADDR(u8 (*)(ocrHint_t*, u32*), getPriorityHint_1_0);
    base->fcts.setAffinity = FUNC_ADDR(u8 (*)(ocrHint_t*, ocrGuid_t), setAffinityHint_1_0);
    base->fcts.getAffinity = FUNC_ADDR(u8 (*)(ocrHint_t*, ocrGuid_t*), getAffinityHint_1_0);
    base->fcts.setDepWeight = FUNC_ADDR(u8 (*)(ocrHint_t*, u32, u32), setDepWeightHint_1_0);
    base->fcts.getDepWeight = FUNC_ADDR(u8 (*)(ocrHint_t*, u32*, u32*), getDepWeightHint_1_0);

    return base;
}

#endif /* ENABLE_HINT_1_0 */

