/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-edt.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime.h"
#include "ocr-errors.h"

#include "utils/profiler/profiler.h"

#define DEBUG_TYPE API

u8 ocrEventCreateParams(ocrGuid_t *guid, ocrEventTypes_t eventType, u16 properties, ocrEventParams_t * params) {

    START_PROFILE(api_EventCreate);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEventCreate(*guid="GUIDSx", eventType=%u, properties=%u)\n", GUIDFS(*guid),
            (u32)eventType, (u32)properties);

    PD_MSG_STACK(msg);
    ocrPolicyDomain_t * pd = NULL;
    u8 returnCode = 0;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EVT_CREATE
    msg.type = PD_MSG_EVT_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = *guid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
#ifdef ENABLE_EXTENSION_PARAMS_EVT
    PD_MSG_FIELD_I(params) = params;
#endif
    PD_MSG_FIELD_I(properties) = properties;
    PD_MSG_FIELD_I(type) = eventType;
    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0) {
        returnCode = PD_MSG_FIELD_O(returnDetail);
        // Leave the GUID unchanged if the error is OCR_EGUIDEXISTS
        if(returnCode != OCR_EGUIDEXISTS) {
            *guid = (returnCode == 0) ? PD_MSG_FIELD_IO(guid.guid) : NULL_GUID;
        }
    } else {
        *guid = NULL_GUID;
    }
#undef PD_MSG
#undef PD_TYPE

    DPRINTF_COND_LVL(((returnCode != 0) && (returnCode != OCR_EGUIDEXISTS)), DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrEventCreate -> %u; GUID: "GUIDSx"\n", returnCode, GUIDFS(*guid), true, OCR_TRACE_TYPE_EVENT, OCR_ACTION_CREATE);
    RETURN_PROFILE(returnCode);

}

u8 ocrEventCreate(ocrGuid_t *guid, ocrEventTypes_t eventType, u16 properties) {
    return ocrEventCreateParams(guid, eventType, properties, NULL);
}

u8 ocrEventDestroy(ocrGuid_t eventGuid) {
    START_PROFILE(api_EventDestroy);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEventDestroy(guid="GUIDSx")\n", GUIDFS(eventGuid));
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EVT_DESTROY
    msg.type = PD_MSG_EVT_DESTROY | PD_MSG_REQUEST;

    PD_MSG_FIELD_I(guid.guid) = eventGuid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(properties) = 0;

    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrEventDestroy(guid="GUIDSx") -> %u\n", GUIDFS(eventGuid), returnCode);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEventSatisfySlot(ocrGuid_t eventGuid, ocrGuid_t dataGuid /*= INVALID_GUID*/, u32 slot) {

    START_PROFILE(api_EventSatisfySlot);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEventSatisfySlot(evt="GUIDSx", data="GUIDSx", slot=%u)\n",
            GUIDFS(eventGuid), GUIDFS(dataGuid), slot);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;

    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(satisfierGuid.guid) = curEdt?curEdt->guid:NULL_GUID;
    PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(guid.guid) = eventGuid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(payload.guid) = dataGuid;
    PD_MSG_FIELD_I(payload.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(slot) = slot;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                    "EXIT ocrEventSatisfySlot(evt="GUIDSx") -> %u\n", GUIDFS(eventGuid), returnCode, true, OCR_TRACE_TYPE_EVENT,
                    OCR_ACTION_SATISFY, dataGuid);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEventSatisfy(ocrGuid_t eventGuid, ocrGuid_t dataGuid /*= INVALID_GUID*/) {
    return ocrEventSatisfySlot(eventGuid, dataGuid, 0);
}

u8 ocrEdtTemplateCreate_internal(ocrGuid_t *guid, ocrEdt_t funcPtr, u32 paramc, u32 depc, char* funcName) {
    START_PROFILE(api_EdtTemplateCreate);

    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEdtTemplateCreate(*guid="GUIDSx", funcPtr=0x%lx, paramc=%d, depc=%d, name=%s)\n",
            GUIDFS(*guid), funcPtr, (s32)paramc, (s32)depc, funcName?funcName:"");

#ifdef OCR_ENABLE_EDT_NAMING
    // Please check that OCR_ENABLE_EDT_NAMING is defined in the app's makefile
    ASSERT(funcName);
#endif
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EDTTEMP_CREATE
    msg.type = PD_MSG_EDTTEMP_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = *guid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(funcPtr) = funcPtr;
    PD_MSG_FIELD_I(paramc) = paramc;
    PD_MSG_FIELD_I(depc) = depc;
    PD_MSG_FIELD_I(properties) = 0;
#ifdef OCR_ENABLE_EDT_NAMING
    {
        u32 t = ocrStrlen(funcName);
        if(t >= OCR_EDT_NAME_SIZE) {
            funcName[OCR_EDT_NAME_SIZE-1] = '\0';
            t = OCR_EDT_NAME_SIZE-1;
        }
        PD_MSG_FIELD_I(funcName) = funcName;
        PD_MSG_FIELD_I(funcNameLen) = t;
    }
#endif

    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if(returnCode == 0) {
        returnCode = PD_MSG_FIELD_O(returnDetail);
        *guid = (returnCode == 0) ? PD_MSG_FIELD_IO(guid.guid) : NULL_GUID;
    } else {
        *guid = NULL_GUID;
    }
#undef PD_MSG
#undef PD_TYPE
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrEdtTemplateCreate -> %u; GUID: "GUIDSx"\n", returnCode, GUIDFS(*guid));
    RETURN_PROFILE(returnCode);
}

u8 ocrEdtTemplateDestroy(ocrGuid_t guid) {
    START_PROFILE(api_EdtTemplateDestroy);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEdtTemplateDestroy(guid="GUIDSx")\n", GUIDFS(guid));
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_EDTTEMP_DESTROY
    msg.type = PD_MSG_EDTTEMP_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(guid.guid) = guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrEdtTemplateDestroy(guid="GUIDSx") -> %u\n", GUIDFS(guid), returnCode);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEdtCreate(ocrGuid_t* edtGuidPtr, ocrGuid_t templateGuid,
                u32 paramc, u64* paramv, u32 depc, ocrGuid_t *depv,
                u16 properties, ocrHint_t *hint, ocrGuid_t *outputEvent) {
    ocrGuid_t edtGuid = (edtGuidPtr != NULL) ? *edtGuidPtr : NULL_GUID;
    START_PROFILE(api_EdtCreate);

    DPRINTF(DEBUG_LVL_INFO,
           "ENTER ocrEdtCreate(*guid="GUIDSx", template="GUIDSx", paramc=%d, paramv=0x%lx"
           ", depc=%d, depv=0x%lx, prop=%u, hint=%p, outEvt="GUIDSx")\n",
           GUIDFS(edtGuid), GUIDFS(templateGuid), (s32)paramc, paramv, (s32)depc, depv,
           (u32)properties, hint,  GUIDFS(outputEvent?*outputEvent:NULL_GUID),
           true, OCR_TRACE_TYPE_EDT, OCR_ACTION_CREATE);

    PD_MSG_STACK(msg);
    ocrPolicyDomain_t * pd = NULL;
    u8 returnCode = 0;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
    if((paramc == EDT_PARAM_UNK) || (depc == EDT_PARAM_UNK)) {
        DPRINTF(DEBUG_LVL_WARN, "error: paramc or depc cannot be set to EDT_PARAM_UNK\n");
        ASSERT(false);
        RETURN_PROFILE(OCR_EINVAL);
    }

    bool reqResponse = false;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_CREATE
    msg.type = PD_MSG_WORK_CREATE | PD_MSG_REQUEST;
    PD_MSG_FIELD_IO(guid.guid) = edtGuid;
    if (edtGuidPtr != NULL) {
        reqResponse = true;
    } else {
        if ((depc != 0) && (depv == NULL)) {
            // Error since we do not return a GUID, dependences can never be added
            DPRINTF(DEBUG_LVL_WARN,"error: NULL-GUID EDT depv not provided\n");
            ASSERT(false);
            RETURN_PROFILE(OCR_EPERM);
        } else if (depc == EDT_PARAM_DEF) {
            // Because we'd like to avoid deguidifying the template here
            // make the creation synchronous if EDT_PARAM_DEF is set.
            DPRINTF(DEBUG_LVL_WARN,"NULL-GUID EDT creation made synchronous: depc is set to EDT_PARAM_DEF\n");
            reqResponse = true;
        } else if (outputEvent != NULL) {
            DPRINTF(DEBUG_LVL_WARN,"NULL-GUID EDT creation made synchronous: EDT has an output-event\n");
            reqResponse = true;
        }
    }
    if (reqResponse) {
        msg.type |= PD_MSG_REQ_RESPONSE;
    }
    ocrFatGuid_t * depvFatGuids = NULL;
    // EDT_DEPV_DELAYED allows to use the older implementation
    // where dependences were always added by the caller instead
    // of the callee
#ifndef EDT_DEPV_DELAYED
    u32 depvSize = ((depv != NULL) && (depc != EDT_PARAM_DEF)) ? depc : 0;
    ocrFatGuid_t depvArray[depvSize];
    depvFatGuids = ((depvSize) ? depvArray : NULL);
    u32 i = 0;
    for(i=0; i<depvSize; i++) {
        depvArray[i].guid = depv[i];
        depvArray[i].metaDataPtr = NULL;
    }
#endif
    PD_MSG_FIELD_IO(guid.guid) = NULL_GUID; // to be set by callee
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    if(outputEvent) {
        PD_MSG_FIELD_IO(outputEvent.guid) = UNINITIALIZED_GUID;
    } else {
        PD_MSG_FIELD_IO(outputEvent.guid) = NULL_GUID;
    }
    PD_MSG_FIELD_IO(outputEvent.metaDataPtr) = NULL;
    PD_MSG_FIELD_IO(paramc) = paramc;
    PD_MSG_FIELD_IO(depc) = depc;
    PD_MSG_FIELD_I(templateGuid.guid) = templateGuid;
    PD_MSG_FIELD_I(templateGuid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(hint) = hint;
    PD_MSG_FIELD_I(parentLatch.guid) = curEdt ? (!(IS_GUID_NULL(curEdt->finishLatch)) ? curEdt->finishLatch : curEdt->parentLatch) : NULL_GUID;
    PD_MSG_FIELD_I(parentLatch.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(paramv) = paramv;
    PD_MSG_FIELD_I(depv) = depvFatGuids;
    PD_MSG_FIELD_I(workType) = EDT_USER_WORKTYPE;
    PD_MSG_FIELD_I(properties) = properties;

    returnCode = pd->fcts.processMessage(pd, &msg, true);
    if ((returnCode == 0) && (reqResponse)) {
        returnCode = PD_MSG_FIELD_O(returnDetail);
    }
    if(returnCode) {
        DPRINTF(DEBUG_LVL_WARN, "EXIT ocrEdtCreate -> %u\n", returnCode);
        RETURN_PROFILE(returnCode);
    }

    // Read the GUID anyway as the EDT may have been assigned one
    // even if the user didn't need it to be returned.
    edtGuid = PD_MSG_FIELD_IO(guid.guid);
    if (edtGuidPtr != NULL) {
        *edtGuidPtr = edtGuid;
    }
    // These should have been resolved
    paramc = PD_MSG_FIELD_IO(paramc);
    depc = PD_MSG_FIELD_IO(depc);

    if(outputEvent)
        *outputEvent = PD_MSG_FIELD_IO(outputEvent.guid);

#ifndef EDT_DEPV_DELAYED
    // We still need to do that in case depc was EDT_PARAM_DEF
    // and the actual number of dependences was unknown then.
    if ((depv != NULL) && (depvSize == 0)) {
#else
    // Delayed addDependence: if guids dependences were provided, add them now.
    if (depv != NULL) {
#endif
        // Please check that # of dependences agrees with depv vector
        ASSERT(!(IS_GUID_NULL(edtGuid)));
        ASSERT(depc != 0);
        u32 i = 0;
        while(i < depc) {
            if(!(IS_GUID_UNINITIALIZED(depv[i]))) {
                // We only add dependences that are not UNINITIALIZED_GUID
                returnCode = ocrAddDependence(depv[i], edtGuid, i, DB_DEFAULT_MODE);
            } else {
                returnCode = 0;
            }
            ++i;
            if(returnCode) {
                RETURN_PROFILE(returnCode);
            }
        }
    }

    if(outputEvent) {
        DPRINTF(DEBUG_LVL_INFO, "EXIT ocrEdtCreate -> 0; GUID: "GUIDSx"; outEvt: "GUIDSx"\n", GUIDFS(edtGuid), GUIDFS(*outputEvent));
    } else {
        DPRINTF(DEBUG_LVL_INFO, "EXIT ocrEdtCreate -> 0; GUID: "GUIDSx"\n", GUIDFS(edtGuid));
    }
    RETURN_PROFILE(0);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrEdtDestroy(ocrGuid_t edtGuid) {
    START_PROFILE(api_EdtDestroy);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrEdtDestory(guid=0x%lx)\n", edtGuid);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_WORK_DESTROY
    msg.type = PD_MSG_WORK_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(guid.guid) = edtGuid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
    PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = pd->fcts.processMessage(pd, &msg, false);
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrEdtDestroy(guid="GUIDSx") -> %u\n", GUIDFS(edtGuid), returnCode);
    RETURN_PROFILE(returnCode);
#undef PD_MSG
#undef PD_TYPE
}

u8 ocrAddDependence(ocrGuid_t source, ocrGuid_t destination, u32 slot,
                    ocrDbAccessMode_t mode) {
    START_PROFILE(api_AddDependence);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrAddDependence(src="GUIDSx", dest="GUIDSx", slot=%u, mode=%d)\n",
            GUIDFS(source), GUIDFS(destination), slot, (s32)mode);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t * curEdt = NULL;
    getCurrentEnv(&pd, NULL, &curEdt, &msg);
    u8 returnCode = 0;
    if(!(IS_GUID_NULL(source))) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_ADD
        msg.type = PD_MSG_DEP_ADD | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(source.guid) = source;
        PD_MSG_FIELD_I(source.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(dest.guid) = destination;
        PD_MSG_FIELD_I(dest.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(slot) = slot;
        PD_MSG_FIELD_IO(properties) = mode;
        PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
        PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
        returnCode = pd->fcts.processMessage(pd, &msg, true);
        DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrAddDependence through PD_MSG_DEP_ADD(src=GUIDSx, dest=GUIDSx) -> %u\n", GUIDFS(source), GUIDFS(destination), returnCode);
#undef PD_MSG
#undef PD_TYPE
    } else {
      //Handle 'NULL_GUID' case here to avoid overhead of
      //going through dep_add and end-up doing the same thing.
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
        msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(satisfierGuid.guid) = curEdt?curEdt->guid:NULL_GUID;
        PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = curEdt;
        PD_MSG_FIELD_I(guid.guid) = destination;
        PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(payload.guid) = NULL_GUID;
        PD_MSG_FIELD_I(payload.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(currentEdt.guid) = curEdt ? curEdt->guid : NULL_GUID;
        PD_MSG_FIELD_I(currentEdt.metaDataPtr) = curEdt;
        PD_MSG_FIELD_I(slot) = slot;
        PD_MSG_FIELD_I(properties) = 0;
        returnCode = pd->fcts.processMessage(pd, &msg, true);
        DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrAddDependence through PD_MSG_DEP_SATISFY(src="GUIDSx", dest="GUIDSx") -> %u\n", GUIDFS(source), GUIDFS(destination), returnCode);
#undef PD_MSG
#undef PD_TYPE
    }
    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrAddDependence(src="GUIDSx", dest="GUIDSx") -> %u\n", GUIDFS(source), GUIDFS(destination), returnCode);
    RETURN_PROFILE(returnCode);
}
