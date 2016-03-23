/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - create an EDT at NULL_GUID through a hint
 */

ocrGuid_t remoteEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Create a data-block and store its affinity inside for future cross-check
    ocrGuid_t remoteEdtTemplateGuid;
    ocrEdtTemplateCreate(&remoteEdtTemplateGuid, remoteEdt, 0, 0);

    // Want to spawn the EDT in the same affinity group the DB is.
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(NULL_GUID));
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, remoteEdtTemplateGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
        EDT_PROP_NONE, &edtHint, NULL);
    return NULL_GUID;
}
