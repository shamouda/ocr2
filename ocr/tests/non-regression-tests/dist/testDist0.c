/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <stdio.h>
#include <assert.h>

#include "ocr.h"

/**
 * DESC: OCR-DIST - create a remote EDT
 */

#define EDT_AFFINITY ((ocrGuid_t) 65)

ocrGuid_t localEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("Hello\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // create local edt that depends on the remote edt, the db is automatically cloned
    ocrGuid_t localEdtTemplateGuid;
    ocrEdtTemplateCreate(&localEdtTemplateGuid, localEdt, 0, 0);

    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, localEdtTemplateGuid, 0, NULL, 0, NULL_GUID,
        EDT_PROP_NONE, EDT_AFFINITY, NULL);

    return NULL_GUID;
}
