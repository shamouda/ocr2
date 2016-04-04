/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test ocrEdtDestruct API call when an EDT report to a finish EDT
 */

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Should never get executed
    ASSERT(false);
    return NULL_GUID;
}

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("ocrShutdown Invoked\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t finishEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t edtGuid;
    ocrGuid_t edtTplGuid;
    ocrEdtTemplateCreate(&edtTplGuid, workEdt, 0, 1);
    ocrEdtCreate(&edtGuid, edtTplGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    PRINTF("Created work EDT "GUIDF"\n", GUIDA(edtGuid));
    // If the child EDT is not properly destroyed and the finish counter
    // is not updated the code should deadlock.
    ocrEdtDestroy(edtGuid);
    PRINTF("Destroyed work EDT "GUIDF"\n", GUIDA(edtGuid));
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t edtGuid;
    ocrGuid_t edtTplGuid;
    ocrGuid_t outputEventGuid;
    ocrEdtTemplateCreate(&edtTplGuid,  finishEdt, 0, 1);
    ocrEdtCreate(&edtGuid, edtTplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_FINISH, NULL_HINT, &outputEventGuid);
    PRINTF("Created finish EDT "GUIDF" with outputEvent "GUIDF"\n", GUIDA(edtGuid), GUIDA(outputEventGuid));
    ocrGuid_t shutGuid;
    ocrGuid_t shutTplGuid;
    ocrEdtTemplateCreate(&shutTplGuid, shutdownEdt, 0, 1);
    // Shutdown to be executed when finish EDT completes

    ocrEdtCreate(&shutGuid, shutTplGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(outputEventGuid, shutGuid, 0, DB_MODE_CONST);
    PRINTF("Created shutdown EDT "GUIDF"\n", GUIDA(shutGuid));

    // Trigger the finish EDT
    ocrAddDependence(NULL_GUID, edtGuid, 0, DB_MODE_CONST);

    return NULL_GUID;
}
