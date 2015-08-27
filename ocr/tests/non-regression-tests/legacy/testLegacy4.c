
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
 * DESC: ocrLegacyBlockProgress that do not provide pointers back to write DB info
 */

// This is the "key" EDT that is responsible for spawning all the workers
ocrGuid_t keyEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    return NULL_GUID;
}

void ocrBlock(ocrConfig_t cfg) {
    ocrGuid_t legacyCtx;
    ocrGuid_t template, handle;
    ocrGuid_t ctrlDep;

    ocrLegacyInit(&legacyCtx, &cfg);
    ocrEdtTemplateCreate(&template, keyEdt, 0, 1);

    ctrlDep = NULL_GUID;
    ocrLegacySpawnOCR(&handle, template, 0, NULL, 1, &ctrlDep, legacyCtx);

    ocrLegacyBlockProgress(handle, NULL, NULL, NULL, LEGACY_PROP_NONE);
    ocrEventDestroy(handle);
    ocrEdtTemplateDestroy(template);
    ocrShutdown();
    ocrLegacyFinalize(legacyCtx, true);
}

int main(int argc, const char *argv[]) {
    ocrConfig_t ocrConfig;
    ocrParseArgs(argc, argv, &ocrConfig);
    PRINTF("Legacy code...\n");
    ocrBlock(ocrConfig);
    PRINTF("Back to legacy code, done.\n");
    return 0;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
