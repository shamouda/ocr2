/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
/**
 * DESC: Add three once event as dependence and satisfy the same order
 */


ocrGuid_t taskForEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int* res = (int*)depv[0].ptr;
    PRINTF("In the taskForEdt with value %"PRId32"\n", (*res));
    ASSERT(*res == 42);
    // This is the last EDT to execute, terminate
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Current thread is '0' and goes on with user code.
    ocrGuid_t e0;
    ocrEventCreate(&e0, OCR_EVENT_ONCE_T, EVT_PROP_TAKES_ARG);
    ocrGuid_t e1;
    ocrEventCreate(&e1, OCR_EVENT_ONCE_T, EVT_PROP_TAKES_ARG);
    ocrGuid_t e2;
    ocrEventCreate(&e2, OCR_EVENT_ONCE_T, EVT_PROP_TAKES_ARG);

    // Creates the EDTa
    ocrGuid_t edtGuid;
    ocrGuid_t taskForEdtTemplateGuid;
    ocrEdtTemplateCreate(&taskForEdtTemplateGuid, taskForEdt, 0 /*paramc*/, 3/*depc*/);
    ocrEdtCreate(&edtGuid, taskForEdtTemplateGuid, EDT_PARAM_DEF, /*paramv=*/NULL, EDT_PARAM_DEF, /*depv=*/NULL,
                 /*properties=*/0, NULL_HINT, /*outEvent=*/NULL);

    // Register a dependence between an event and an edt
    ocrAddDependence(e0, edtGuid, 0, DB_MODE_CONST);
    ocrAddDependence(e1, edtGuid, 1, DB_MODE_CONST);
    ocrAddDependence(e2, edtGuid, 2, DB_MODE_CONST);

    int *k;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid,(void **) &k,
                sizeof(int), /*flags=*/DB_PROP_NONE,
                /*location=*/NULL_HINT,
                NO_ALLOC);
    *k = 42;

    // Satisfy event's chain head
    ocrEventSatisfy(e0, dbGuid);
    ocrEventSatisfy(e1, dbGuid);
    ocrEventSatisfy(e2, dbGuid);

    return NULL_GUID;
}
