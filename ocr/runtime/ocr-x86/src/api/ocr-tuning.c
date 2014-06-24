/**
* @brief Tuning hints implementation for OCR
*/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-task.h"
#include "ocr-tuning.h"
#include "ocr-hint.h"

#define DEBUG_TYPE API

#if OCR_HINT_VERSION >= 100

u8 ocrHintCreate(ocrGuid_t *guid, ocrHintTypes_t hintType) {
    START_PROFILE(api_HintCreate);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_CREATE
    msg.type = PD_MSG_HINT_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = *guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(type) = hintType;
    PD_MSG_FIELD(properties) = HINT_PROP_USER;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0)
        *guid = PD_MSG_FIELD(guid.guid);
    else
        *guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrHintSetPriority(ocrGuid_t guid, u32 priority) {
    START_PROFILE(api_HintEdt);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_OP
    msg.type = PD_MSG_HINT_OP | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid.guid) = guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = HINT_PROP_SET_PRIORITY | HINT_PROP_USER;
    PD_MSG_FIELD(arg1.guid) = (ocrGuid_t)priority;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrHintSetDependenceWeight(ocrGuid_t guid, u32 depNum, u32 depWeight) {
    START_PROFILE(api_HintEdt);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_OP
    msg.type = PD_MSG_HINT_OP | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid.guid) = guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = HINT_PROP_SET_DEP_WEIGHT | HINT_PROP_USER;
    PD_MSG_FIELD(arg1.guid) = (ocrGuid_t)depNum;
    PD_MSG_FIELD(arg2.guid) = (ocrGuid_t)depWeight;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrSetHint(ocrGuid_t guid, ocrGuid_t hint) {
    START_PROFILE(api_HintSet);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_SET
    msg.type = PD_MSG_HINT_SET | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid.guid) = guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(hint.guid) = hint;
    PD_MSG_FIELD(hint.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = HINT_PROP_USER;
    PD_MSG_FIELD(returnDetail) = 0;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrGetHint(ocrGuid_t guid, ocrGuid_t *hint) {
    START_PROFILE(api_HintGet);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_HINT_GET
    msg.type = PD_MSG_HINT_GET | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(hint.guid) = *hint;
    PD_MSG_FIELD(hint.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = HINT_PROP_USER;
    PD_MSG_FIELD(returnDetail) = 0;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0)
        *hint = PD_MSG_FIELD(hint.guid);
    else
        *hint = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

#else /* OCR_HINT_VERSION < 100 */

u8 ocrHintCreate(ocrGuid_t *guid, ocrHintTypes_t hintType) {
    DPRINTF(DEBUG_LVL_WARN, "Funtion ocrHintCreate not supported\n");
    *guid = NULL_GUID;
    return OCR_ENOTSUP;
}

u8 ocrHintSetPriority(ocrGuid_t guid, u32 priority) {
    DPRINTF(DEBUG_LVL_WARN, "Funtion ocrHintSetPriority not supported!\n");
    return OCR_ENOTSUP;
}

u8 ocrHintSetDependenceWeight(ocrGuid_t guid, u32 depNum, u32 depWeight) {
    DPRINTF(DEBUG_LVL_WARN, "Funtion ocrHintSetDependenceWeight not supported!\n");
    return OCR_ENOTSUP;
}

u8 ocrSetHint(ocrGuid_t guid, ocrGuid_t hint) {
    DPRINTF(DEBUG_LVL_WARN, "Funtion ocrSetHint not supported!\n");
    return OCR_ENOTSUP;
}

u8 ocrGetHint(ocrGuid_t guid, ocrGuid_t *hint) {
    DPRINTF(DEBUG_LVL_WARN, "Funtion ocrGetHint not supported!\n");
    return OCR_ENOTSUP;
}

#endif

