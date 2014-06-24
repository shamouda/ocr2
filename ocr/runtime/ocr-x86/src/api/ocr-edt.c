/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-edt.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime.h"

#include "utils/profiler/profiler.h"

u8 ocrEventCreate(ocrGuid_t *guid, ocrEventTypes_t eventType, bool takesArg) {
    START_PROFILE(api_EventCreate);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t * pd = NULL;
    u8 returnCode = 0;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EVT_CREATE
    msg.type = PD_MSG_EVT_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = *guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = takesArg;
    PD_MSG_FIELD(type) = eventType;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0)
        *guid = PD_MSG_FIELD(guid.guid);
    else
        *guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrEventDestroy(ocrGuid_t eventGuid) {
    START_PROFILE(api_EventDestroy);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EVT_DESTROY
    msg.type = PD_MSG_EVT_DESTROY | PD_MSG_REQUEST;

    PD_MSG_FIELD(guid.guid) = eventGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(properties) = 0;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;

    u8 toReturn = pd->fcts.processMessage(pd, &msg, false);
    RETURN_PROFILE(toReturn);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEventSatisfySlot(ocrGuid_t eventGuid, ocrGuid_t dataGuid /*= INVALID_GUID*/, u32 slot) {

    START_PROFILE(api_EventSatisfySlot);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;

    PD_MSG_FIELD(guid.guid) = eventGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(payload.guid) = dataGuid;
    PD_MSG_FIELD(payload.metaDataPtr) = NULL;
    PD_MSG_FIELD(slot) = slot;
    PD_MSG_FIELD(mapping) = curEdt ? curEdt->mapping : INVALID_LOCATION;
    PD_MSG_FIELD(properties) = 0;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
    u8 toReturn = pd->fcts.processMessage(pd, &msg, false);
    RETURN_PROFILE(toReturn);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEventSatisfy(ocrGuid_t eventGuid, ocrGuid_t dataGuid /*= INVALID_GUID*/) {
    return ocrEventSatisfySlot(eventGuid, dataGuid, 0);
}

u8 ocrEdtTemplateCreate_internal(ocrGuid_t *guid, ocrEdt_t funcPtr, u32 paramc, u32 depc, const char* funcName) {
    START_PROFILE(api_EdtTemplateCreate);
#ifdef OCR_ENABLE_EDT_NAMING
    ASSERT(funcName);
#endif
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EDTTEMP_CREATE
    msg.type = PD_MSG_EDTTEMP_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = *guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(funcPtr) = funcPtr;
    PD_MSG_FIELD(paramc) = paramc;
    PD_MSG_FIELD(depc) = depc;
    PD_MSG_FIELD(funcName) = funcName;
    PD_MSG_FIELD(funcNameLen) = ocrStrlen(funcName);
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;

    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0)
        *guid = PD_MSG_FIELD(guid.guid);
    else
        *guid = NULL_GUID;
#undef PD_MSG
#undef PD_TYPE
    RETURN_PROFILE(returnCode);
}

u8 ocrEdtTemplateDestroy(ocrGuid_t guid) {
    START_PROFILE(api_EdtTemplateDestroy);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EDTTEMP_DESTROY
    msg.type = PD_MSG_EDTTEMP_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid.guid) = guid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
    u8 toReturn = pd->fcts.processMessage(pd, &msg, false);
    RETURN_PROFILE(toReturn);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEdtCreate(ocrGuid_t* edtGuid, ocrGuid_t templateGuid,
                u32 paramc, u64* paramv, u32 depc, ocrGuid_t *depv,
                u16 properties, ocrGuid_t affinity, ocrGuid_t *outputEvent) {

    START_PROFILE(api_EdtCreate);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t * pd = NULL;
    u8 returnCode = 0;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_CREATE
    msg.type = PD_MSG_WORK_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = *edtGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(templateGuid.guid) = templateGuid;
    PD_MSG_FIELD(templateGuid.metaDataPtr) = NULL;
    PD_MSG_FIELD(affinity.guid) = affinity;
    PD_MSG_FIELD(affinity.metaDataPtr) = NULL;
    PD_MSG_FIELD(outputEvent.metaDataPtr) = NULL;
    if(outputEvent) {
        PD_MSG_FIELD(outputEvent.guid) = UNINITIALIZED_GUID;
    } else {
        PD_MSG_FIELD(outputEvent.guid) = NULL_GUID;
    }
    PD_MSG_FIELD(paramv) = paramv;
    PD_MSG_FIELD(paramc) = paramc;
    PD_MSG_FIELD(depc) = depc;
    PD_MSG_FIELD(properties) = properties;
    PD_MSG_FIELD(workType) = EDT_WORKTYPE;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;

    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode) {
        RETURN_PROFILE(returnCode);
    }

    *edtGuid = PD_MSG_FIELD(guid.guid);
    paramc = PD_MSG_FIELD(paramc);
    depc = PD_MSG_FIELD(depc);
    if(outputEvent)
        *outputEvent = PD_MSG_FIELD(outputEvent.guid);

    // If guids dependences were provided, add them now
    // TODO: This should probably just send the messages
    // to the PD
    if(depv != NULL) {
        ASSERT(depc != 0);
        u32 i = 0;
        while(i < depc) {
            // FIXME: Not really good. We would need to undo maybe
            returnCode = ocrAddDependence(depv[i], *edtGuid, i, DB_DEFAULT_MODE);
            ++i;
            if(returnCode) {
                RETURN_PROFILE(returnCode);
            }
        }
    }
    RETURN_PROFILE(0);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEdtDestroy(ocrGuid_t edtGuid) {
    START_PROFILE(api_EdtDestroy);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_DESTROY
    msg.type = PD_MSG_WORK_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid.guid) = edtGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
    u8 toReturn = pd->fcts.processMessage(pd, &msg, false);
    RETURN_PROFILE(toReturn);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrAddDependence(ocrGuid_t source, ocrGuid_t destination, u32 slot,
                    ocrDbAccessMode_t mode) {
    START_PROFILE(api_AddDependence);
    ocrPolicyMsg_t msg;
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
    if(source != NULL_GUID) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_ADD
        msg.type = PD_MSG_DEP_ADD | PD_MSG_REQUEST;
        PD_MSG_FIELD(source.guid) = source;
        PD_MSG_FIELD(source.metaDataPtr) = NULL;
        PD_MSG_FIELD(dest.guid) = destination;
        PD_MSG_FIELD(dest.metaDataPtr) = NULL;
        PD_MSG_FIELD(slot) = slot;
        PD_MSG_FIELD(properties) = mode;
        PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
        PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
        PD_MSG_FIELD(mapping) = curEdt ? curEdt->mapping : INVALID_LOCATION;
#undef PD_MSG
#undef PD_TYPE
    } else {
      //Handle 'NULL_GUID' case here to avoid overhead of
      //going through dep_add and end-up doing the same thing.
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
        msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
        PD_MSG_FIELD(guid.guid) = destination;
        PD_MSG_FIELD(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD(payload.guid) = NULL_GUID;
        PD_MSG_FIELD(payload.metaDataPtr) = NULL;
        PD_MSG_FIELD(slot) = slot;
        PD_MSG_FIELD(mapping) = curEdt ? curEdt->mapping : INVALID_LOCATION;
        PD_MSG_FIELD(properties) = 0;
        PD_MSG_FIELD(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
        PD_MSG_FIELD(currentEdt.metaDataPtr) = curEdt;
#undef PD_MSG
#undef PD_TYPE
    }
    u8 toReturn = pd->fcts.processMessage(pd, &msg, false);
    RETURN_PROFILE(toReturn);
}
