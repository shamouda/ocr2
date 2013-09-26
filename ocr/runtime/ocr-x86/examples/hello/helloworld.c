/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <stdio.h>

#include "ocr.h"

ocrGuid_t db;
ocrGuid_t template, edt;

ocrGuid_t bar ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("I'm a bar\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t foo ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("I'm a foo\n");
    ocrEdtTemplateCreate(&template, bar, 0, 0);
    ocrEdtCreate(&edt, template, 0, NULL, 0, NULL, EDT_PROP_NONE, NULL_GUID, NULL);
    return NULL_GUID;
}

ocrGuid_t mainEdt ( u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("Hello !\n");
    ocrEdtTemplateCreate(&template, foo, 0, 0);
    void *val = NULL;
    ocrDbCreate(&db, &val, 888, DB_PROP_NO_ACQUIRE, NULL_GUID, NO_ALLOC);
    ocrEdtCreate(&edt, template, 0, NULL, 0, NULL, EDT_PROP_NONE, NULL_GUID, NULL);
    return NULL_GUID;
}
