/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test ocrEdtDestruct API call when EDT has an output-event setup
 */

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Should never get executed
    ASSERT(false);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t workEdtGuid;
    ocrGuid_t workEdtTplGuid;
    ocrGuid_t outputEventGuid;
    ocrEdtTemplateCreate(&workEdtTplGuid, workEdt, 0, 1);
    ocrEdtCreate(&workEdtGuid, workEdtTplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, &outputEventGuid);
    ocrEdtDestroy(workEdtGuid);
    ocrShutdown();
    return NULL_GUID;
}
