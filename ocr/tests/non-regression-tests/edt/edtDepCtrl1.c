/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Control dependence with ocrAddDependence/DB_MODE_NULL
 *       Passing a DB guid is legal and the EDT won't acquire it
 */

ocrGuid_t otherEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 1);
    ASSERT(ocrGuidIsNull(depv[0].guid));
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t tplGuid;
    ocrEdtTemplateCreate(&tplGuid, otherEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, tplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrEdtTemplateDestroy(tplGuid);
    ocrGuid_t db1Guid;
    u64 * db1Ptr;
    ocrDbCreate(&db1Guid, (void**) &db1Ptr, sizeof(u64), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    db1Ptr[0] = 1;
    ocrDbRelease(db1Guid);
    ocrAddDependence(db1Guid, edtGuid, 0, DB_MODE_NULL);
    return NULL_GUID;
}
