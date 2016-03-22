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
 * DESC: Invoke ocrLegacySpawnOCR twice in a row
 */

// This is a worker EDT
ocrGuid_t workerEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Worker here, this would do a portion of the parallel workload\n");
    return NULL_GUID;
}

// This is the "key" EDT that is responsible for spawning all the workers
ocrGuid_t keyEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t templateGuid;
    ocrEdtTemplateCreate(&templateGuid, workerEdt, 0, 0);

    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, templateGuid, 0, NULL, 0, NULL, EDT_PROP_FINISH, NULL_HINT, NULL);
    return NULL_GUID;
}

// Demonstrates 2 "blocks of OCR" run back to back from within the same legacy context
void ocrBlock(ocrConfig_t cfg) {
    ocrGuid_t legacyCtx;
    ocrGuid_t template, handle;

    ocrGuid_t ctrlDep;
    ocrGuid_t outputGuid;
    void *result;
    u64 size;

    ocrLegacyInit(&legacyCtx, &cfg);
    ocrEdtTemplateCreate(&template, keyEdt, 0, 1);

    ctrlDep = NULL_GUID;
    ocrLegacySpawnOCR(&handle, template, 0, NULL, 1, &ctrlDep, legacyCtx);

    ocrLegacyBlockProgress(handle, &outputGuid, &result, &size, LEGACY_PROP_NONE);
    ASSERT(ocrGuidIsNull(outputGuid));
    ASSERT(result == NULL);
    ASSERT(size == 0);
    ocrEventDestroy(handle);

    PRINTF("Let's try again...\n");

    ctrlDep = NULL_GUID;
    ocrLegacySpawnOCR(&handle, template, 0, NULL, 1, &ctrlDep, legacyCtx);

    ocrLegacyBlockProgress(handle, &outputGuid, &result, &size, LEGACY_PROP_NONE);
    ASSERT(ocrGuidIsNull(outputGuid));
    ASSERT(result == NULL);
    ASSERT(size == 0);
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
    PRINTF("Let's try again with a fresh legacy block\n");
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
