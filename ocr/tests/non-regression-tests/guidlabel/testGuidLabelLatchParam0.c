/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

#if defined ENABLE_EXTENSION_LABELING && ENABLE_EXTENSION_PARAMS_EVT
#include "extensions/ocr-labeling.h"

/**
 * DESC: Test latch event parameters in the context of labeled GUIDs.
 */

#define NB_EDT 10

ocrGuid_t mapFunc(ocrGuid_t startGuid, u64 stride, s64* params, s64* tuple) {
    return (ocrGuid_t)(tuple[0]*stride + startGuid);
}

ocrGuid_t shutEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t createEvtEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t mapGuid = (ocrGuid_t) paramv[0];
    ocrGuid_t shutGuid = (ocrGuid_t) paramv[1];
    s64 val = 0;
    ocrGuid_t evtGuid;
    ocrGuidFromLabel(&evtGuid, mapGuid, &val);
    ocrEventParams_t latchParams;
    latchParams.EVENT_LATCH.counter = NB_EDT;
    PRINTF("About to try creating the event\n");
    u8 retCode = ocrEventCreateParams(&evtGuid, OCR_EVENT_LATCH_T, GUID_PROP_IS_LABELED | GUID_PROP_CHECK, &latchParams);
    if (retCode != OCR_EGUIDEXISTS) {
        PRINTF("Succeeded\n");
        // First to create the event
        ocrAddDependence(evtGuid, shutGuid, 0, DB_MODE_RO);
    }
    PRINTF("Satisfy\n");
    ocrEventSatisfySlot(evtGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t mapGuid = NULL_GUID;
    ocrGuidMapCreate(&mapGuid, 0, mapFunc, NULL, 10, GUID_USER_EVENT_LATCH);

    ocrGuid_t templGuid;
    ocrEdtTemplateCreate(&templGuid, shutEdt, 0, 1);
    ocrGuid_t shutGuid;
    ocrEdtCreate(&shutGuid, templGuid, 0, NULL, 1, NULL, EDT_PROP_NONE, NULL_GUID, NULL);

    ocrGuid_t crtEvtTmplGuid;
    ocrEdtTemplateCreate(&crtEvtTmplGuid, createEvtEdt, 2, 0);
    u64 nparamv[2];
    nparamv[0] = (u64) mapGuid;
    nparamv[1] = (u64) shutGuid;
    u32 i = 0;
    while (i < NB_EDT) {
        ocrEdtCreate(NULL, crtEvtTmplGuid, 2, nparamv, 0, NULL, EDT_PROP_NONE, NULL_GUID, NULL);
        i++;
    }
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_LABELING & ENABLE_EXTENSION_PARAMS_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif