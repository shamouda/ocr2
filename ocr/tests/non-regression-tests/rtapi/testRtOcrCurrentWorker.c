/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: RT-API: Test 'ocrCurrentWorkerGuid'
 */

// Only tested when OCR runtime API is available
#ifdef ENABLE_EXTENSION_RTITF

#include "extensions/ocr-runtime-itf.h"

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // No real right or wrong here, just check the call doesn't crash
    ocrGuid_t workerGuid = ocrCurrentWorkerGuid();
    PRINTF("Current worker GUID is "GUIDF"\n", GUIDA(workerGuid));
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
