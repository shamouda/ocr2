#include "ocr.h"
#include "extensions/ocr-legacy.h"

#include <stdio.h>
#include <stdlib.h>

// This is a worker EDT

ocrGuid_t workerEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Worker here, this would do a portion of the parallel workload\n");
    return NULL_GUID;
}

// This is the "key" EDT that is responsible for spawning all the workers

ocrGuid_t keyEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t templateGuid;
    ocrEdtTemplateCreate(&templateGuid, workerEdt, 0, 0);

    ocrGuid_t edtGuid, evtGuid;
    ocrEdtCreate(&edtGuid, templateGuid, 0, NULL, 0, NULL_GUID, EDT_PROP_FINISH, NULL_GUID, &evtGuid);
    return evtGuid;
}

// Demonstrates 2 "blocks of OCR" run back to back from within the same legacy context

void ocrBlock(ocrConfig_t cfg)
{

    ocrGuid_t legacyCtx;
    ocrGuid_t  edt, template, output_event;
    ocrGuid_t wait_event;

    ocrGuid_t ctrlDep;
    ocrGuid_t outputGuid;
    void *result;
    u64 size;

    ocrLegacyInit(&legacyCtx, &cfg);
    ocrEdtTemplateCreate(&template, keyEdt, 0, 1);

    ctrlDep = NULL_GUID;
    ocrLegacySpawnOCR(&output_event, template, 0, NULL, 1, &ctrlDep, legacyCtx);

    ocrLegacyBlockProgress(output_event, &outputGuid, &result, &size);
    ocrEventDestroy(output_event);


    printf("Let's try again...\n");


    ctrlDep = NULL_GUID;
    ocrLegacySpawnOCR(&output_event, template, 0, NULL, 1, &ctrlDep, legacyCtx);

    ocrLegacyBlockProgress(output_event, &outputGuid, &result, &size);
    ocrEventDestroy(output_event);

    ocrEdtTemplateDestroy(template);
    ocrLegacyFinalize(&legacyCtx, false);
}

int main(int argc, char *argv[])
{

    ocrConfig_t cfg;

    cfg.userArgc = argc;
    cfg.userArgv = argv;
    cfg.iniFile = getenv("OCR_CONFIG");

    printf("Legacy code...\n");

    ocrBlock(cfg);

    printf("Let's try again with a fresh legacy block\n");

    ocrBlock(cfg);

    printf("Back to legacy code, done.\n");

    return 0;
}
