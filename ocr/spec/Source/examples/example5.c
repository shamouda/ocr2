/*
 *  This file is subject to the license agreement located in the file LICENSE
 *  and cannot be distributed without it. This notice cannot be
 *  removed or modified.
 */

/* Example usage of EW (Exclusive-Write)
 * data block access mode in OCR
 *
 * Implements the following dependence graph:
 *
 *       mainEdt
 *       [ DB ]
 *      / |     \
 * (RW)/  |(RW)  \(EW)
 *    /   |       \
 * EDT1  EDT2    EDT3
 *    \   |      /
 *     \  |     /
 *      \ |    /
 *       [ DB ]
 *     shutdownEdt
 *
 */

#include "ocr.h"

#define NB_ELEM_DB 20

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 * data = (u64 *) depv[3].ptr;
    u32 i = 0;
    while (i < NB_ELEM_DB) {
        PRINTF("%"PRId32" ",data[i]);
        i++;
    }
    PRINTF("\n");
    ocrDbDestroy(depv[3].guid);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t writerEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 * data = (u64 *) depv[0].ptr;
    u64 lb = paramv[0];
    u64 ub = paramv[1];
    u64 value = paramv[2];
    u32 i = lb;
    while (i < ub) {
        data[i] += value;
        i++;
    }
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    void * dbPtr;
    ocrGuid_t dbGuid;
    u32 nbElem = NB_ELEM_DB;
    ocrDbCreate(&dbGuid, &dbPtr, sizeof(u64) * NB_ELEM_DB, 0, NULL_HINT, NO_ALLOC);
    u64 i = 0;
    int * data = (int *) dbPtr;
    while (i < nbElem) {
        data[i] = 0;
        i++;
    }
    ocrDbRelease(dbGuid);

    ocrGuid_t shutdownEdtTemplateGuid;
    ocrEdtTemplateCreate(&shutdownEdtTemplateGuid, shutdownEdt, 0, 4);
    ocrGuid_t shutdownGuid;
    ocrEdtCreate(&shutdownGuid, shutdownEdtTemplateGuid, 0, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(dbGuid, shutdownGuid, 3, DB_MODE_CONST);

    ocrGuid_t writeEdtTemplateGuid;
    ocrEdtTemplateCreate(&writeEdtTemplateGuid, writerEdt, 3, 2);

    ocrGuid_t eventStartGuid;
    ocrEventCreate(&eventStartGuid, OCR_EVENT_ONCE_T, false);

    // RW '1' from 0 to N/2 (potentially concurrent with writer 1, but different range)
    ocrGuid_t oeWriter0Guid;
    ocrGuid_t writer0Guid;
    u64 writerParamv0[3] = {0, NB_ELEM_DB/2, 1};
    ocrEdtCreate(&writer0Guid, writeEdtTemplateGuid, EDT_PARAM_DEF, writerParamv0, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, &oeWriter0Guid);
    ocrAddDependence(oeWriter0Guid, shutdownGuid, 0, false);
    ocrAddDependence(dbGuid, writer0Guid, 0, DB_MODE_RW);
    ocrAddDependence(eventStartGuid, writer0Guid, 1, DB_MODE_CONST);

    // RW '2' from N/2 to N (potentially concurrent with writer 0, but different range)
    ocrGuid_t oeWriter1Guid;
    ocrGuid_t writer1Guid;
    u64 writerParamv1[3] = {NB_ELEM_DB/2, NB_ELEM_DB, 2};
    ocrEdtCreate(&writer1Guid, writeEdtTemplateGuid, EDT_PARAM_DEF, writerParamv1, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, &oeWriter1Guid);
    ocrAddDependence(oeWriter1Guid, shutdownGuid, 1, false);
    ocrAddDependence(dbGuid, writer1Guid, 0, DB_MODE_RW);
    ocrAddDependence(eventStartGuid, writer1Guid, 1, DB_MODE_CONST);

    // EW '3' from N/4 to 3N/4
    ocrGuid_t oeWriter2Guid;
    ocrGuid_t writer2Guid;
    u64 writerParamv2[3] = {NB_ELEM_DB/4, (NB_ELEM_DB/4)*3, 3};
    ocrEdtCreate(&writer2Guid, writeEdtTemplateGuid, EDT_PARAM_DEF, writerParamv2, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, &oeWriter2Guid);
    ocrAddDependence(oeWriter2Guid, shutdownGuid, 2, false);
    ocrAddDependence(dbGuid, writer2Guid, 0, DB_MODE_EW);
    ocrAddDependence(eventStartGuid, writer2Guid, 1, DB_MODE_CONST);

    ocrEventSatisfy(eventStartGuid, NULL_GUID);

    return NULL_GUID;
}
