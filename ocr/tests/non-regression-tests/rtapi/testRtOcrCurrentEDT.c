/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: RT-API: Test 'currentEdtUserGet'
 */

// Only tested when OCR runtime API is available
#ifdef ENABLE_EXTENSION_RTITF

#include "extensions/ocr-runtime-itf.h"

ocrGuid_t taskForEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t edtGuid = *((ocrGuid_t*)depv[0].ptr);
    ocrGuid_t currentEdt = currentEdtUserGet();
    // Compare edtGuid passed down and what's returned by the runtime
    ASSERT(ocrGuidIsEq(currentEdt, edtGuid));
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("RT API Test\n");
    ocrGuid_t eventGuid;
    ocrEventCreate(&eventGuid, OCR_EVENT_STICKY_T, true);

    // Creates the EDT
    ocrGuid_t edtGuid;
    ocrGuid_t taskForEdtTemplateGuid;
    ocrEdtTemplateCreate(&taskForEdtTemplateGuid, taskForEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrEdtCreate(&edtGuid, taskForEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, 0, NULL_HINT, NULL);

    // Build a data-block and pass down the edtGuid
    ocrGuid_t *k;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid,(void **) &k,
                sizeof(ocrGuid_t), /*flags=*/0,
                /*location=*/NULL_HINT,
                NO_ALLOC);
    *k = edtGuid;

    // Pass down the db to the edt
    ocrAddDependence(dbGuid, edtGuid, 0, DB_MODE_CONST);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("No RT API\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
