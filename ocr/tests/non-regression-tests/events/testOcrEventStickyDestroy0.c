/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

#define N 100

/**
 * DESC: Create an event with a bunch of dependences. The first dependence
 *       schedules an EDT that destroys the event
 */

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // This is the last EDT to execute, terminate
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t delEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t evt = (ocrGuid_t) paramv[0];
    ocrEventDestroy(evt);
    return NULL_GUID;
}

ocrGuid_t otherEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    return NULL_GUID;
}

ocrGuid_t finishEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t eventGuid;
    ocrEventCreate(&eventGuid, OCR_EVENT_STICKY_T, false);

    ocrGuid_t delTplGuid;
    ocrEdtTemplateCreate(&delTplGuid, delEdt, 1 /*paramc*/, 1 /*depc*/);
    ocrGuid_t delEdtGuid;
    ocrEdtCreate(&delEdtGuid, delTplGuid, 1, (u64 *) &eventGuid, 1, &eventGuid, 0, NULL_GUID, NULL);

    ocrGuid_t otherTplGuid;
    ocrEdtTemplateCreate(&otherTplGuid, otherEdt, 0 /*paramc*/, 1 /*depc*/);
    u32 i = 0;
    while (i < N) {
        ocrGuid_t otherEdtGuid;
        ocrEdtCreate(&otherEdtGuid, otherTplGuid, 0, NULL,
                     1, &eventGuid, 0, NULL_GUID, NULL);
        i++;
    }

    ocrEventSatisfy(eventGuid, NULL_GUID);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    ocrGuid_t tplGuid;
    ocrEdtTemplateCreate(&tplGuid, finishEdt, 0/*paramc*/, 1 /*depc*/);
    ocrGuid_t outEvent;
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, tplGuid, 0, NULL, 1, NULL, EDT_PROP_FINISH, NULL_GUID, &outEvent);

    ocrEdtTemplateCreate(&tplGuid, shutdownEdt, 0/*paramc*/, 1 /*depc*/);
    ocrGuid_t sEdtGuid;
    ocrEdtCreate(&sEdtGuid, tplGuid, 0, NULL, 1, &outEvent, 0, NULL_GUID, NULL);

    ocrAddDependence(NULL_GUID, edtGuid, 0, DB_MODE_RO);

    return NULL_GUID;
}

