/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Try to exercise bug #855 where satisfying the last dependence with a DB
 *       and a previous one that's an event does an out of bound on the EDT
 *       dependence list when iterating the frontier
 */

#ifndef N
#define N 4
#endif

ocrGuid_t otherEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t addEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t edtGuid = {.guid = paramv[0]};
    ocrGuid_t dbGuid =  {.guid = paramv[1]};
    ocrAddDependence(dbGuid, edtGuid, N, DB_MODE_RO);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t tplGuid;
    ocrEdtTemplateCreate(&tplGuid, otherEdt, 0 /*paramc*/, N+1 /*depc*/);
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, tplGuid, 0, NULL, N+1, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    ocrEdtTemplateDestroy(tplGuid);

    ocrGuid_t dbGuid;
    u64 * db1Ptr;
    ocrDbCreate(&dbGuid, (void**) &db1Ptr, sizeof(u64), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbGuid);
    ocrGuid_t evtGuid;
    ocrEventCreate(&evtGuid, OCR_EVENT_STICKY_T, true);
    ocrAddDependence(evtGuid, edtGuid, 0, DB_MODE_RO);


    ocrGuid_t tpl2Guid;
    ocrEdtTemplateCreate(&tpl2Guid, addEdt, 2, 0);
    ocrGuid_t edt2Guid;
    u64 nparamv[2];
    nparamv[0] = (u64) edtGuid.guid;
    nparamv[1] = (u64) dbGuid.guid;
    ocrEdtCreate(&edt2Guid, tpl2Guid, 2, nparamv, 0,  NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    u32 i = 1;
    while (i < N) {
        ocrAddDependence(dbGuid, edtGuid, i, DB_MODE_RO);
        i++;
    }
    ocrEventSatisfy(evtGuid, NULL_GUID);
    ocrEdtTemplateDestroy(tpl2Guid);
    return NULL_GUID;
}
