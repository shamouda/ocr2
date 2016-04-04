#include "ocr.h"

/**
 * DESC: Create a null-GUID EDT
 */

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Everything went OK\n");
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0 /*paramc*/, 0 /*depc*/);
    ocrEdtCreate(NULL, terminateEdtTemplateGuid, 0, NULL, 0, NULL,
                 /*properties=*/EDT_PROP_NONE, NULL_HINT, /*outEvent=*/ NULL);
    return NULL_GUID;
}
