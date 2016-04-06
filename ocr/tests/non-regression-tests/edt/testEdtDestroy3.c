/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test ocrEdtDestruct API call when an EDT is a finish EDT
 */

ocrGuid_t finishEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Should never get executed
    ASSERT(false);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t edtGuid;
    ocrGuid_t edtTplGuid;
    ocrGuid_t outputEventGuid;
    ocrEdtTemplateCreate(&edtTplGuid,  finishEdt, 0, 1);
    ocrEdtCreate(&edtGuid, edtTplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_FINISH, NULL_HINT, &outputEventGuid);
    ocrEdtDestroy(edtGuid);
    ocrShutdown();
    return NULL_GUID;
}
