/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test depv with UNINITIALIZED_GUID values
 */

ocrGuid_t otherEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 * u1 = depv[0].ptr;
    u64 * u2 = depv[1].ptr;
    ASSERT(u1[0] == 1);
    ASSERT(u2[0] == 2);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t tplGuid;
    ocrEdtTemplateCreate(&tplGuid, otherEdt, 0 /*paramc*/, 2 /*depc*/);
    ocrGuid_t edtGuid;
    ocrGuid_t ndepv[2];
    ndepv[0] = UNINITIALIZED_GUID;
    ndepv[1] = UNINITIALIZED_GUID;
    ocrEdtCreate(&edtGuid, tplGuid, 0, NULL, 2, ndepv, EDT_PROP_NONE, NULL_HINT, NULL);
    ocrEdtTemplateDestroy(tplGuid);

    ocrGuid_t db1Guid;
    u64 * db1Ptr;
    ocrDbCreate(&db1Guid, (void**) &db1Ptr, sizeof(u64), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    db1Ptr[0] = 1;
    ocrDbRelease(db1Guid);

    ocrGuid_t db2Guid;
    u64 * db2Ptr;
    ocrDbCreate(&db2Guid, (void**) &db2Ptr, sizeof(u64), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    db2Ptr[0] = 2;
    ocrDbRelease(db2Guid);

    // First add last dependence, then first
    ocrAddDependence(db2Guid, edtGuid, 1, DB_MODE_RO);
    ocrAddDependence(db1Guid, edtGuid, 0, DB_MODE_RO);

    return NULL_GUID;
}
