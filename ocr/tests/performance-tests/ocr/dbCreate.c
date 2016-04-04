#include "perfs.h"
#include "ocr.h"

// DESC: Create DB_NBS EDTs that each keep creating & destroying DBs, each of size DB_SZ
// TIME: Duration of create & destroy
// FREQ: Done 'NB_ITERS' times
//
// VARIABLES:
// - NB_ITERS
// - DB_NBS
// - DB_SZ

// Time the destroys by default
#ifndef TIME_DESTROY
#define TIME_DESTROY 1
#endif

ocrGuid_t wrapupEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u32 i;
    u64 * timersPtr = (u64 *) depv[DB_NBS].ptr;
    u64 t0, t1, t2;

    t0 = 0;
    t1 = 0;
    t2 = 0;

    for(i = 0; i<DB_NBS*3; i+=3) {
        t0 += timersPtr[i];
        t1 += timersPtr[i+1];
        t2 += timersPtr[i+2];
    }

    PRINTF("Overall: DB Create %"PRId32"\t DB Release %"PRId32"\t DB Destroy %"PRId32"\n", t0/DB_NBS, t1/DB_NBS, t2/DB_NBS);

    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t edtCode(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 * timersPtr = (u64 *) depv[0].ptr;
    u64 index = paramv[0] * 3;
    u32 i = 0;
    u64 * syncPtr = (u64 *) depv[1].ptr;
    timestamp_t t0, t1, t2, t3;
    ocrGuid_t dbGuid;
    void *dbPtr = NULL;
    u8 retval = 0;

    timersPtr[index+0] = 0;
    timersPtr[index+1] = 0;
    timersPtr[index+2] = 0;

    for(i = 0; i<NB_ITERS; i++) {

        dbGuid = NULL_GUID;
        get_time(&t0);
        ocrDbCreate(&dbGuid, (void **)&dbPtr, DB_SZ, 0, NULL_HINT, NO_ALLOC);
        // Check that the DB creates succeeded
        ASSERT(!ocrGuidIsNull(dbGuid));
        ASSERT(dbPtr != NULL);

        get_time(&t1);
        ocrDbRelease(dbGuid);
        get_time(&t2);
#if  TIME_DESTROY
        ocrDbDestroy(dbGuid);
        get_time(&t3);
#endif

        timersPtr[index+0] += elapsed_usec(&t0, &t1);
        timersPtr[index+1] += elapsed_usec(&t1, &t2);
#if TIME_DESTROY
        timersPtr[index+2] += elapsed_usec(&t2, &t3);
#endif

        // If errors or some other thread's done, bail
        if(((*(u64 *)syncPtr) == 1) || retval)
            break;
    }

    *(u64 *)syncPtr = 1;

    if(i==0)
        PRINTF("Invalid run! Please check that the number of EDTs generated match number of workers\n");
    else {
        // Now average them out
        timersPtr[index+0] /= i;
        timersPtr[index+1] /= i;
        timersPtr[index+2] /= i;

        PRINTF("Thread %"PRId32": %"PRId32" iterations; Times: %"PRId64" %"PRId64" %"PRId64"\n", paramv[0], i, timersPtr[index], timersPtr[index+1], timersPtr[index+2]);
    }

    return NULL_GUID;
}

// One paramv: [driverIteration]
// One depv  : [dbTimer]
ocrGuid_t driverEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 driverIteration = paramv[0];
    ocrGuid_t dbTimerGuid = depv[0].guid;
    u64 i;

    ocrGuid_t edtCodeTemplateGuid;
    ocrEdtTemplateCreate(&edtCodeTemplateGuid, edtCode, 1, 2);
    ocrGuid_t finishTemplateGuid;
    ocrEdtTemplateCreate(&finishTemplateGuid, wrapupEdt, 0, DB_NBS+1);

    ocrGuid_t edtWrapupGuid;
    ocrEdtCreate(&edtWrapupGuid, finishTemplateGuid,
                 0, NULL, 1+DB_NBS, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    ocrAddDependence(dbTimerGuid, edtWrapupGuid, DB_NBS, DB_MODE_RW);

    for(i=0; i<DB_NBS; i++) {
        ocrGuid_t edtGuid;
        ocrGuid_t eventGuid;
        ocrEdtCreate(&edtGuid, edtCodeTemplateGuid,
                     1, &i, 2, NULL, EDT_PROP_NONE, NULL_HINT, &eventGuid);

        // Add timer dependence
        ocrAddDependence(eventGuid, edtWrapupGuid, i, DB_MODE_RW);
        ocrAddDependence(dbTimerGuid, edtGuid, 0, DB_MODE_RW);
        ocrAddDependence(depv[1].guid, edtGuid, 1, DB_MODE_RW);
    }
    ocrEdtTemplateDestroy(edtCodeTemplateGuid);
    ocrEdtTemplateDestroy(finishTemplateGuid);

    return NULL_GUID;
}


ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    u64 * dbTimerPtr;
    ocrGuid_t dbTimerGuid;
    u64 *syncPtr;
    ocrGuid_t syncGuid;

    ocrDbCreate(&dbTimerGuid, (void **)&dbTimerPtr, sizeof(u64)*3*DB_NBS, 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbTimerGuid);

    ocrDbCreate(&syncGuid, (void **)&syncPtr, sizeof(u64), 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(syncGuid);

    *(u64 *)syncPtr = 0;

    ocrGuid_t edtDriverTemplateGuid;
    ocrEdtTemplateCreate(&edtDriverTemplateGuid, driverEdt, 1, 2);

    u64 paramvDriverEdt[1];
    paramvDriverEdt[0] = 0; //driverIteration

    ocrGuid_t driverEdtGuid;
    ocrEdtCreate(&driverEdtGuid, edtDriverTemplateGuid,
                 1, paramvDriverEdt, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(dbTimerGuid, driverEdtGuid, 0, DB_MODE_RW);
    ocrAddDependence(syncGuid, driverEdtGuid, 1, DB_MODE_RW);
    ocrEdtTemplateDestroy(edtDriverTemplateGuid);

    return NULL_GUID;
}
