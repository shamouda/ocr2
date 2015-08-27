/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: RT-API: Test 'ocrInformLegacyCodeBlocking'
 */

// Only tested when OCR runtime API is available
#ifdef ENABLE_EXTENSION_RTITF

#include "extensions/ocr-runtime-itf.h"

#define N 10

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    // This call is used to let the runtime know the EDT is actively waiting
    // for something and that the runtime may perform chores or whatever the
    // scheduler implementation could do (maybe execute a new EDT)
    int i = 0;
    while (i < N) {
        ocrInformLegacyCodeBlocking();
        i++;
    }
    // Condition reached continue work

    ocrShutdown();
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("No RT API\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
