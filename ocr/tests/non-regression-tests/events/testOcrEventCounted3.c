/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Counted-event: one dependence is satisfied and the descendent EDT add remaining deps
 */

#ifdef ENABLE_EXTENSION_COUNTED_EVT

#ifndef NB_EVT_COUNTED_DEPS
#define NB_EVT_COUNTED_DEPS 4
#endif

ocrGuid_t shtEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}


ocrGuid_t taskForEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t shtGuid = {.guid=paramv[0]};
    if (paramc == 2) {
        // shtGuid is the edt
        ocrGuid_t evtGuid = {.guid=paramv[1]};
        ocrGuid_t tplEdt;
        ocrEdtTemplateCreate(&tplEdt, taskForEdt, 1 , 1);
        u32 i;
        ocrAddDependence(NULL_GUID, shtGuid, 0, DB_MODE_RO);
        for (i=1; i < NB_EVT_COUNTED_DEPS; i++) {
            ocrEventParams_t params;
            params.EVENT_COUNTED.nbDeps = 1;
            ocrGuid_t shtEvtGuid;
            ocrEventCreateParams(&shtEvtGuid, OCR_EVENT_COUNTED_T, false, &params);
            ocrGuid_t edtGuid;
            ocrEdtCreate(&edtGuid, tplEdt, EDT_PARAM_DEF, (u64*) &shtEvtGuid, EDT_PARAM_DEF, NULL,
                         EDT_PROP_NONE, NULL_HINT, NULL);
            // The EDT will be immediately eligible for scheduling since evtGuid has
            // already been satisfied by the parent EDT
            ocrAddDependence(evtGuid, edtGuid, 0, DB_MODE_RO);
            // Same thing here, potentially the EDT has ran and satisfied this shtEvtGuid
            // but it's ok since it's a counted event, it stays around its single dependence
            // is added
            ocrAddDependence(shtEvtGuid, shtGuid, i, DB_MODE_RO);
        }
    } else {
        // shtGuid is the event
        ocrEventSatisfy(shtGuid, NULL_GUID);
    }
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // This is the tested event
    ocrEventParams_t params;
    params.EVENT_COUNTED.nbDeps = NB_EVT_COUNTED_DEPS;
    ocrGuid_t evtGuid;
    ocrEventCreateParams(&evtGuid, OCR_EVENT_COUNTED_T, false, &params);

    ocrGuid_t tplSht;
    ocrEdtTemplateCreate(&tplSht, shtEdt, 0 , NB_EVT_COUNTED_DEPS);
    ocrGuid_t shtEdtGuid;
    ocrEdtCreate(&shtEdtGuid, tplSht, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);

    ocrGuid_t tplEdt;
    ocrEdtTemplateCreate(&tplEdt, taskForEdt, 2 , 1);
    u64 nparamv[2];
    nparamv[0] = (u64) shtEdtGuid.guid;
    nparamv[1] = (u64) evtGuid.guid;
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, tplEdt, EDT_PARAM_DEF, nparamv, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);

    // Register a dependence between the counted event and the edt
    ocrAddDependence(evtGuid, edtGuid, 0, DB_MODE_RO);

    // satisfy the event, auto-destroy can only be triggered after all deps have been added.
    // the edt that's triggered by this satisfy will in turn add the remaining dependences.
    ocrEventSatisfy(evtGuid, NULL_GUID);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_COUNTED_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
