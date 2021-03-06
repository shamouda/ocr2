/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"

/**
 * DESC: Test runtime checks paramv is NULL if paramc is EDT_PARAM_DEF => 0
 */

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Everything went OK\n");
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtGuid;
    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0 /*paramc*/, 0 /*depc*/);
    u64 nparamv = 1;
    u8 res = ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid, EDT_PARAM_DEF, &nparamv, 0, NULL,
                 /*properties=*/EDT_PROP_FINISH, NULL_HINT, /*outEvent=*/ NULL);
    ASSERT(!res);
    if(res) {
        ocrAbort(res);
    }
    return NULL_GUID;
}
