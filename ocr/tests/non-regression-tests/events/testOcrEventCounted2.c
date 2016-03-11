/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Counted-event: event is satisfied, then add all deps
 *       Also use counted event for the sink edt scheduling
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
    ocrGuid_t evtGuid = {.guid=paramv[0]};
    ocrEventSatisfy(evtGuid, NULL_GUID);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // This is the tested event
    ocrEventParams_t params;
    params.EVENT_COUNTED.nbDeps = NB_EVT_COUNTED_DEPS;
    ocrGuid_t evtGuid;
    ocrEventCreateParams(&evtGuid, OCR_EVENT_COUNTED_T, false, &params);
    u32 i = 0;

    // Setup completion events for sub-edts, also use counted here for fun
    ocrGuid_t evtGuids[NB_EVT_COUNTED_DEPS];
    for (i=0; i < NB_EVT_COUNTED_DEPS; i++) {
        ocrEventParams_t params;
        params.EVENT_COUNTED.nbDeps = 1;
        ocrEventCreateParams(&evtGuids[i], OCR_EVENT_COUNTED_T, false, &params);
    }

    ocrGuid_t tplSht;
    ocrEdtTemplateCreate(&tplSht, shtEdt, 0 , NB_EVT_COUNTED_DEPS);
    ocrGuid_t shtEdtGuid;
    ocrEdtCreate(&shtEdtGuid, tplSht, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);

    ocrGuid_t edtGuids[NB_EVT_COUNTED_DEPS];
    ocrGuid_t tplEdt;
    ocrEdtTemplateCreate(&tplEdt, taskForEdt, 1 , 1);

    // satisfy the event, auto-destroy can only be triggered after all deps have been added
    ocrEventSatisfy(evtGuid, NULL_GUID);

    // add half the deps of the counted event
    for (i=0; i < NB_EVT_COUNTED_DEPS; i++) {
        ocrEdtCreate(&edtGuids[i], tplEdt, EDT_PARAM_DEF, (u64*) &evtGuids[i], EDT_PARAM_DEF, NULL,
                     EDT_PROP_NONE, NULL_HINT, NULL);
        // Register a dependence between an event and an edt
        ocrAddDependence(evtGuid, edtGuids[i], 0, DB_MODE_CONST);
        ocrAddDependence(evtGuids[i], shtEdtGuid, i, DB_MODE_CONST);
    }

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_COUNTED_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
