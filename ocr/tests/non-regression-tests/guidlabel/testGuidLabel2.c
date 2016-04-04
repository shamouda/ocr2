/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

#ifdef ENABLE_EXTENSION_LABELING
#include "extensions/ocr-labeling.h"

/**
 * DESC: labeled GUID used to concurrently create an event
 */

#define NB_EDT 100

ocrGuid_t mapFunc(ocrGuid_t startGuid, u64 stride, s64* params, s64* tuple) {
    return (ocrGuid_t)(tuple[0]*stride + startGuid);
}

ocrGuid_t shutEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t db = NULL_GUID;
    int i = 0;
    while (i < NB_EDT) {
        if (depv[i].guid != NULL_GUID) {
            ASSERT(ocrGuidIsNull(db));
            db = depv[i].guid;
        }
        i++;
    }
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t createEvtEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t mapGuid = (ocrGuid_t) paramv[0];
    s64 val = 0;
    ocrGuid_t evtGuid;
    ocrGuidFromLabel(&evtGuid, mapGuid, &val);
    u8 retCode = ocrEventCreate(&evtGuid, OCR_EVENT_STICKY_T, GUID_PROP_IS_LABELED | GUID_PROP_CHECK);
    if (retCode == OCR_EGUIDEXISTS) {
        return NULL_GUID;
    } else {
        // Return a DB to mark this EDT has succeeded
        ocrGuid_t dbGuid;
        void * dbPtr;
        ocrDbCreate(&dbGuid, &dbPtr, sizeof(u64), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
        ocrDbRelease(dbGuid);
        return dbGuid;
    }
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t mapGuid = NULL_GUID;
    ocrGuidMapCreate(&mapGuid, 0, mapFunc, NULL, 10, GUID_USER_EVENT_STICKY);

    ocrGuid_t startGuid;
    ocrEventCreate(&startGuid, OCR_EVENT_ONCE_T, EVT_PROP_NONE);

    ocrGuid_t templGuid;
    ocrEdtTemplateCreate(&templGuid, shutEdt, 0, NB_EDT);
    ocrGuid_t shutGuid;
    ocrEdtCreate(&shutGuid, templGuid, 0, NULL, NB_EDT, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    ocrGuid_t crtEvtTmplGuid;
    ocrEdtTemplateCreate(&crtEvtTmplGuid, createEvtEdt, 1, 1);
    u64 nparamv = (u64) mapGuid;
    u32 i = 0;
    while (i < NB_EDT) {
        ocrGuid_t outputEvtGuid;
        ocrGuid_t edtGuid;
        ocrEdtCreate(&edtGuid, crtEvtTmplGuid, 1, &nparamv, 1, &startGuid, EDT_PROP_NONE, NULL_HINT, &outputEvtGuid);
        ocrAddDependence(outputEvtGuid, shutGuid, i, DB_MODE_RO);
        i++;
    }
    ocrEventSatisfy(startGuid, NULL_GUID);
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_LABELING not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
