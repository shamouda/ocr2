#include "ocr.h"
#include "extensions/ocr-lib.h"

#include <stdio.h>
#include <stdlib.h>

ocrGuid_t workerEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Worker here, this would do a portion of the parallel workload\n");
    return NULL_GUID;
}

ocrGuid_t keyEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t templateGuid;
    ocrEdtTemplateCreate(&templateGuid, workerEdt, 0, 0);

    ocrGuid_t edtGuid, evtGuid;
    ocrEdtCreate(&edtGuid, templateGuid, 0, NULL, 0, NULL_GUID, EDT_PROP_FINISH, NULL_GUID, &evtGuid);
    return evtGuid;
}

int main(int argc, char *argv[])
{

    ocrConfig_t cfg;
    ocrGuid_t legacyCtx;


    cfg.userArgc = argc;
    cfg.userArgv = argv;
    cfg.iniFile = getenv("OCR_CONFIG");

    ocrGuid_t  edt, template, output_event;
    ocrGuid_t wait_event;

    printf("Legacy code...\n");

    // Begin OCR portions
    ocrInitLegacy(&legacyCtx, &cfg);
    ocrEventCreate(&wait_event, OCR_EVENT_STICKY_T, 0);

    ocrEdtTemplateCreate(&template, keyEdt, 0, 0);

    ocrSpawnOCR(&output_event, template, 0, NULL, 0, NULL_GUID, legacyCtx);
    ocrAddDependence(output_event, wait_event, 0, DB_MODE_ITW);
    ocrEdtTemplateDestroy(template);

    ocrLegacyBlockProgress(wait_event, NULL, NULL, NULL);
    ocrEventDestroy(wait_event);

    ocrFinalizeLegacy(&legacyCtx);
    // Done with OCR

    printf("Legacy code...\n");
    printf("Trying again...\n");

    // Begin OCR portions
    ocrInitLegacy(&legacyCtx, &cfg);
    ocrEventCreate(&wait_event, OCR_EVENT_STICKY_T, 0);

    ocrEdtTemplateCreate(&template, keyEdt, 0, 0);

    ocrSpawnOCR(&output_event, template, 0, NULL, 0, NULL_GUID, legacyCtx);
    ocrAddDependence(output_event, wait_event, 0, DB_MODE_ITW);
    ocrEdtTemplateDestroy(template);

    ocrLegacyBlockProgress(wait_event, NULL, NULL, NULL);
    ocrEventDestroy(wait_event);

    ocrFinalizeLegacy(&legacyCtx);
    // Done with OCR

    printf("Legacy code...\n");

    ocrShutdown();

    return 0;
}
