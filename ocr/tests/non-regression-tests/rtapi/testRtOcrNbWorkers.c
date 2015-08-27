/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: RT-API: Test 'ocrNbWorkers'
 */

// Only tested when OCR runtime API is available
#ifdef ENABLE_EXTENSION_RTITF

#include "extensions/ocr-runtime-itf.h"

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 nbW = ocrNbWorkers();
    // Can't really check for actual value here so just look at boundaries.
    ASSERT(nbW > 0);
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
