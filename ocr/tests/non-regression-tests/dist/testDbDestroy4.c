
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Create EDT that destroys a DB passed as dependence without explicit release
 */

#define TYPE_ELEM_DB int
#define NB_ELEM_DB 20

ocrGuid_t readerEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dbGuid = (ocrGuid_t) depv[0].guid;
    PRINTF("[remote] readerEdt: executing, depends on DB guid "GUIDF" \n", GUIDA(dbGuid));
    ocrDbDestroy(dbGuid);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Create a DB
    void * dbPtr;
    ocrGuid_t dbGuid;
    u64 nbElem = NB_ELEM_DB;
    ocrDbCreate(&dbGuid, &dbPtr, sizeof(TYPE_ELEM_DB) * NB_ELEM_DB, 0, NULL_HINT, NO_ALLOC);
    int v = 1;
    int i = 0;
    int * data = (int *) dbPtr;
    while (i < nbElem) {
        data[i] = v++;
        i++;
    }
    // Intentionally not releasing the DB
    PRINTF("[local] mainEdt: local DB guid is "GUIDF", dbPtr=%p\n", GUIDA(dbGuid), dbPtr);

    // create local edt that depends on the remote edt, the db is automatically cloned
    ocrGuid_t readerEdtTemplateGuid;
    ocrEdtTemplateCreate(&readerEdtTemplateGuid, readerEdt, 0, 1);

    ocrGuid_t readerEdtGuid;
    ocrEdtCreate(&readerEdtGuid, readerEdtTemplateGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(dbGuid, readerEdtGuid, 0, DB_MODE_CONST);
    return NULL_GUID;
}
