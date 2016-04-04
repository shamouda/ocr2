/*
 *  This file is subject to the license agreement located in the file LICENSE
 *  and cannot be distributed without it. This notice cannot be
 *  removed or modified.
 */

/* Example of a "fork-join" pattern in OCR
 *
 * Implements the following dependence graph:
 *
 *   mainEdt
 *   /    \
 * fun1   fun2
 *   \    /
 * shutdownEdt
 *
 */

#include "ocr.h"

ocrGuid_t fun1(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int* k;
    ocrGuid_t db_guid;
    ocrDbCreate(&db_guid,(void **) &k, sizeof(int), 0, NULL_HINT, NO_ALLOC);
    k[0]=1;
    PRINTF("Hello from fun1, sending k = %"PRId32"\n",*k);
    return db_guid;
}

ocrGuid_t fun2(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int* k;
    ocrGuid_t db_guid;
    ocrDbCreate(&db_guid,(void **) &k, sizeof(int), 0, NULL_HINT, NO_ALLOC);
    k[0]=2;
    PRINTF("Hello from fun2, sending k = %"PRId32"\n",*k);
    return db_guid;
}

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Hello from shutdownEdt\n");
    int* data1 = (int*) depv[0].ptr;
    int* data2 = (int*) depv[1].ptr;
    PRINTF("Received data1 = %"PRId32", data2 = %"PRId32"\n", *data1, *data2);
    ocrDbDestroy(depv[0].guid);
    ocrDbDestroy(depv[1].guid);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Starting mainEdt\n");
    ocrGuid_t edt1_template, edt2_template, edt3_template;
    ocrGuid_t edt1, edt2, edt3, outputEvent1, outputEvent2;

    //Create templates for the EDTs
    ocrEdtTemplateCreate(&edt1_template, fun1, 0, 1);
    ocrEdtTemplateCreate(&edt2_template, fun2, 0, 1);
    ocrEdtTemplateCreate(&edt3_template, shutdownEdt, 0, 2);

    //Create the EDTs
    ocrEdtCreate(&edt1, edt1_template, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, &outputEvent1);
    ocrEdtCreate(&edt2, edt2_template, EDT_PARAM_DEF, NULL, EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, &outputEvent2);
    ocrEdtCreate(&edt3, edt3_template, EDT_PARAM_DEF, NULL, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    //Setup dependences for the shutdown EDT
    ocrAddDependence(outputEvent1, edt3, 0, DB_MODE_CONST);
    ocrAddDependence(outputEvent2, edt3, 1, DB_MODE_CONST);

    //Start execution of the parallel EDTs
    ocrAddDependence(NULL_GUID, edt1, 0, DB_DEFAULT_MODE);
    ocrAddDependence(NULL_GUID, edt2, 0, DB_DEFAULT_MODE);
    return NULL_GUID;
}
