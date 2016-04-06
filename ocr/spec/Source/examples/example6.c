/*
 *  This file is subject to the license agreement located in the file LICENSE
 *  and cannot be distributed without it. This notice cannot be
 *  removed or modified.
 */

/* Example to show how DB guids can be passed through another DB.
 * Note: DB contents can be accessed by an EDT only when they arrive
 * in a dependence slot.
 *
 * Implements the following dependence graph:
 *
 *     mainEdt
 *     [ DB1 ]
 *        |
 *       EDT1
 *        |
 *     [ DB0 ]
 *   shutdownEdt
 *
 */

#include "ocr.h"

#define VAL 42

ocrGuid_t exampleEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t *dbPtr = (ocrGuid_t*)depv[0].ptr;
    ocrGuid_t passedDb = dbPtr[0];
    PRINTF("Passing DB: "GUIDF"\n", GUIDA(passedDb));
    ocrDbDestroy(depv[0].guid);
    return passedDb;
}

ocrGuid_t awaitingEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 i;
    u32 *dbPtr = (u32*)depv[0].ptr;
    PRINTF("Received: %"PRIu32"\n", dbPtr[0]);
    ocrDbDestroy(depv[0].guid);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u32 i;

    // Create DBs
    u32* ptr0;
    ocrGuid_t* ptr1;
    ocrGuid_t db0Guid, db1Guid;
    ocrDbCreate(&db0Guid, (void**)&ptr0, sizeof(u32), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    ocrDbCreate(&db1Guid, (void**)&ptr1, sizeof(ocrGuid_t), DB_PROP_NONE, NULL_HINT, NO_ALLOC);
    ptr0[0] = VAL;
    ptr1[0] = db0Guid;
    PRINTF("Sending: %"PRIu32" in DB: "GUIDF"\n", ptr0[0], GUIDA(db0Guid));
    ocrDbRelease(db0Guid);
    ocrDbRelease(db1Guid);

    // Create Middle EDT
    ocrGuid_t exampleTemplGuid, exampleEdtGuid, exampleEventGuid;
    ocrEdtTemplateCreate(&exampleTemplGuid, exampleEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrEdtCreate(&exampleEdtGuid, exampleTemplGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
        EDT_PROP_NONE, NULL_HINT, &exampleEventGuid);

    // Create AWAIT EDT
    ocrGuid_t awaitingTemplGuid, awaitingEdtGuid;
    ocrEdtTemplateCreate(&awaitingTemplGuid, awaitingEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrEdtCreate(&awaitingEdtGuid, awaitingTemplGuid, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL,
        EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(exampleEventGuid, awaitingEdtGuid, 0, DB_DEFAULT_MODE);

    // START Middle EDT
    ocrAddDependence(db1Guid, exampleEdtGuid, 0, DB_DEFAULT_MODE);

    return NULL_GUID;
}
