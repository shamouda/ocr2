/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sal.h"
#include "ocr-errors.h"
#include "ocr-runtime-hints.h"
#include "extensions/ocr-hints.h"

#define DEBUG_TYPE API

u64 ocrHintPropIndexStart[] = {
    0,
    OCR_HINT_EDT_PROP_START,
    OCR_HINT_DB_PROP_START,
    OCR_HINT_EVT_PROP_START,
    OCR_HINT_GROUP_PROP_START
};

u64 ocrHintPropIndexEnd[] = {
    0,
    OCR_HINT_EDT_PROP_END,
    OCR_HINT_DB_PROP_END,
    OCR_HINT_EVT_PROP_END,
    OCR_HINT_GROUP_PROP_END
};

#define OCR_HINT_CHECK(hint, property)                                          \
do {                                                                            \
    if (hint->type == OCR_HINT_UNDEF_T ||                                       \
        property <= ocrHintPropIndexStart[hint->type] ||                        \
        property >= ocrHintPropIndexEnd[hint->type]) {                          \
        DPRINTF(DEBUG_LVL_WARN, "EXIT: Unsupported hint type or property\n");	\
        return OCR_EINVAL;                                                      \
    }                                                                           \
} while(0);

#define HINT_DBG_WARN_MSG "WARNING: Current build of OCR is not configured to use the hints API\n"

u8 ocrHintInit(ocrHint_t *hint, ocrHintType_t hintType) {
#ifdef ENABLE_HINTS
    hint->type = hintType;
    hint->propMask = 0;
    switch (hintType) {
    case OCR_HINT_EDT_T:
        {
            OCR_HINT_FIELD(hint, OCR_HINT_EDT_PRIORITY) = 0;
            OCR_HINT_FIELD(hint, OCR_HINT_EDT_SLOT_MAX_ACCESS) = ((u64)-1);
            OCR_HINT_FIELD(hint, OCR_HINT_EDT_AFFINITY) = ((u64)-1);
            OCR_HINT_FIELD(hint, OCR_HINT_EDT_PHASE) = ((u64)-1);
        }
        break;
    case OCR_HINT_DB_T:
        {
            OCR_HINT_FIELD(hint, OCR_HINT_DB_MEM_AFFINITY) = 0;
        }
        break;
    case OCR_HINT_EVT_T:
    case OCR_HINT_GROUP_T:
        break;
    default:
        return OCR_EINVAL;
    }
    return 0;
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

u8 ocrSetHintValue(ocrHint_t *hint, ocrHintProp_t hintProp, u64 value) {
#ifdef ENABLE_HINTS
    OCR_HINT_CHECK(hint, hintProp);
    hint->propMask |= OCR_HINT_BIT_MASK(hint, hintProp);
    OCR_HINT_FIELD(hint, hintProp) = value;
    return 0;
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

u8 ocrUnsetHintValue(ocrHint_t *hint, ocrHintProp_t hintProp) {
#ifdef ENABLE_HINTS
    OCR_HINT_CHECK(hint, hintProp);
    hint->propMask &= ~OCR_HINT_BIT_MASK(hint, hintProp);
    return 0;
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

u8 ocrGetHintValue(ocrHint_t *hint, ocrHintProp_t hintProp, u64 *value) {
#ifdef ENABLE_HINTS
    OCR_HINT_CHECK(hint, hintProp);
    if ((hint->propMask & OCR_HINT_BIT_MASK(hint, hintProp)) == 0)
        return OCR_ENOENT;
    *value = OCR_HINT_FIELD(hint, hintProp);
    return 0;
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

u8 ocrSetHint(ocrGuid_t guid, ocrHint_t *hint) {
#ifdef ENABLE_HINTS
    if (hint->type == OCR_HINT_UNDEF_T) {
        DPRINTF(DEBUG_LVL_WARN, "EXIT ocrSetHint: Invalid hint type\n");
        return OCR_EINVAL;
    }

    START_PROFILE(api_SetHint);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrSetHint(guid=0x%lx)\n", guid);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_SET
    msg.type = PD_MSG_HINT_SET | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(guid.guid) = guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(hint) = *hint;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrSetHint(guid=0x%lx) -> %u\n", guid, returnCode);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

u8 ocrGetHint(ocrGuid_t guid, ocrHint_t *hint) {
#ifdef ENABLE_HINTS
    if (hint->type == OCR_HINT_UNDEF_T) {
        DPRINTF(DEBUG_LVL_WARN, "EXIT ocrGetHint: Invalid hint type\n");
        return OCR_EINVAL;
    }

    START_PROFILE(api_SetHint);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrSetHint(guid=0x%lx)\n", guid);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_GET
    msg.type = PD_MSG_HINT_GET | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_I(guid.guid) = guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_IO(hint) = *hint;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, true);
    if (returnCode == 0) {
        returnCode = PD_MSG_FIELD_O(returnDetail);
        if (returnCode == 0) {
            *hint = PD_MSG_FIELD_IO(hint);
        }
    }
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrSetHint(guid=0x%lx) -> %u\n", guid, returnCode);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
#else
    DPRINTF(DEBUG_LVL_WARN, HINT_DBG_WARN_MSG);
    return OCR_EINVAL;
#endif
}

