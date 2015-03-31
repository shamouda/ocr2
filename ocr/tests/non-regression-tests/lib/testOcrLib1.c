/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */




#include "ocr.h"

// Only tested when OCR legacy interface is available
#ifdef OCR_LEGACY_ITF

#include "extensions/ocr-legacy.h"

/**
 * DESC: OCR-legacy, init, edt does shutdown, finalize
 */

ocrGuid_t rootEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Running rootEdt\n");
    ocrShutdown();
    return NULL_GUID;
}

int main(int argc, const char * argv[]) {
    ocrConfig_t ocrConfig;
    ocrGuid_t legacyCtx;
    ocrParseArgs(argc, argv, &ocrConfig);
    ocrLegacyInit(&legacyCtx, &ocrConfig);
    ocrGuid_t rootEdtGuid;
    ocrGuid_t rootEdtTemplateGuid;
    ocrEdtTemplateCreate(&rootEdtTemplateGuid, rootEdt, 0 /*paramc*/, 0 /*depc*/);
    ocrEdtCreate(&rootEdtGuid, rootEdtTemplateGuid, EDT_PARAM_DEF, /*paramv=*/NULL, EDT_PARAM_DEF, /*depv=*/NULL,
                 /*properties=*/0, NULL_GUID, /*outEvent=*/NULL);
    ocrLegacyFinalize(legacyCtx, true);
    return 0;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
