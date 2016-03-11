/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Pass program argument to shutdown EDT
 */

ocrGuid_t otherEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 argc;
    void *programArg = depv[0].ptr;
    u64* dbAsU64 = (u64*)programArg;
    argc = dbAsU64[0];
    ASSERT(argc == 2);

    ocrGuid_t tplGuid;
    ocrEdtTemplateCreate(&tplGuid, otherEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, tplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(depv[0].guid, edtGuid, 0, DB_MODE_CONST);
    return NULL_GUID;
}
