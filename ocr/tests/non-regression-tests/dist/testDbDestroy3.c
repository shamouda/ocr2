
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Create EDT that destroys a DB (prior release in caller)
 */

#define TYPE_ELEM_DB int
#define NB_ELEM_DB 20

ocrGuid_t destroyEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t * dbGuidArray = (ocrGuid_t *) depv[0].ptr;
    PRINTF("destroyEdt: destroy DB guid "GUIDF" \n", GUIDA(dbGuidArray[0]));
    ocrDbDestroy(dbGuidArray[0]);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Create a DB
    ocrGuid_t * dbPtrArray;
    ocrGuid_t dbGuidArray;
    ocrDbCreate(&dbGuidArray, (void **)&dbPtrArray, sizeof(ocrGuid_t), 0, NULL_HINT, NO_ALLOC);
    // Set the other db guid in the guid array
    ocrGuid_t dbGuid;
    void * dbPtr;
    ocrDbCreate(&dbGuid, &dbPtr, sizeof(TYPE_ELEM_DB) * NB_ELEM_DB, 0, NULL_HINT, NO_ALLOC);
    dbPtrArray[0] = dbGuid;
    PRINTF("mainEdt: local DB guid is "GUIDF"\n", GUIDA(dbPtrArray[0]));
    ocrDbRelease(dbGuidArray);
    ocrDbRelease(dbGuid);
    // create local edt that depends on the remote edt, the db is automatically cloned
    ocrGuid_t destroyEdtTemplateGuid;
    ocrEdtTemplateCreate(&destroyEdtTemplateGuid, destroyEdt, 0, 1);

    ocrGuid_t destroyEdtGuid;
    ocrEdtCreate(&destroyEdtGuid, destroyEdtTemplateGuid, 0, NULL, 1, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);

    ocrAddDependence(dbGuidArray, destroyEdtGuid, 0, DB_MODE_CONST);
    return NULL_GUID;
}
