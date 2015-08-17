/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#include "ocr.h"

// Only tested when OCR library interface is available
#ifdef ENABLE_EXTENSION_LEGACY

#include "extensions/ocr-legacy.h"

/**
 * DESC: init, shutdown, finalize
 */

int main(int argc, const char * argv[]) {
    ocrConfig_t ocrConfig;
    ocrGuid_t legacyCtx;
    ocrParseArgs(argc, argv, &ocrConfig);
    ocrLegacyInit(&legacyCtx, &ocrConfig);
    PRINTF("Running\n");
    ocrShutdown();
    ocrLegacyFinalize(legacyCtx, true);
    return 0;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
