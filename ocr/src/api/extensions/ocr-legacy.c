/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_EXTENSION_LEGACY

#include "extensions/ocr-lib.h"
#include "debug.h"
#include "extensions/ocr-legacy.h"
#include "ocr-hal.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"

#warning Experimental OCR legacy support enabled

extern void freeUpRuntime(void);

// WARNING: The event MUST be sticky. DO NOT WAIT ON A LATCH EVENT!!!
ocrGuid_t ocrWait(ocrGuid_t eventToYieldForGuid) {
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrEvent_t *eventToYieldFor = NULL;
    ocrFatGuid_t result;

    getCurrentEnv(&pd, NULL, NULL, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
    msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = eventToYieldForGuid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = KIND_GUIDPROP | RMETA_GUIDPROP;
    RESULT_PROPAGATE2(pd->fcts.processMessage(pd, &msg, true), ERROR_GUID);
    eventToYieldFor = (ocrEvent_t *)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_MSG
#undef PD_TYPE

    ASSERT(eventToYieldFor->kind == OCR_EVENT_STICKY_T ||
           eventToYieldFor->kind == OCR_EVENT_IDEM_T);

    do {
        hal_pause();
        result.guid = ERROR_GUID;
        result = pd->eventFactories[0]->fcts[eventToYieldFor->kind].get(eventToYieldFor);
    } while(result.guid == ERROR_GUID);

    return result.guid;
}

void ocrInitLegacy(ocrGuid_t *legacyContext, ocrConfig_t * ocrConfig) {
    // currently ignoring context

    if(ocrConfig->iniFile == NULL)
        PRINTF("Set OCR_CONFIG to point to OCR configuration file\n");
    ASSERT(ocrConfig->iniFile);
    ocrInit(ocrConfig);
}


u8 ocrFinalizeLegacy(ocrGuid_t legacyContext) {
    freeUpRuntime();
    return 0;
}

u8 ocrSpawnOCR(ocrGuid_t* handle, ocrGuid_t finishEdtTemplate, u64 paramc, u64* paramv,
               u64 depc, ocrGuid_t depv, ocrGuid_t legacyContext) {

    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, finishEdtTemplate, paramc, paramv, depc, depv, EDT_PROP_FINISH, NULL_GUID, handle);
    return 0;
}

u8 ocrLegacyBlockProgress(ocrGuid_t handle, ocrGuid_t* guid, void** result, u64* size) {
    ocrWait(handle);
    // For now, return NULL - to be fixed
    if(guid) guid = NULL;
    if(result) result = NULL;
    if(size) *size = 0;
    return 0;
}

#endif /* ENABLE_EXTENSION_LEGAGY */
