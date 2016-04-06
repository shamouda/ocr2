#include "perfs.h"
#include "ocr.h"

// DESC: Create an EDT that has 'DB_NBS' DB dependences
// TIME: Add dependence between 'DB_NBS' DBs and an EDT
// FREQ: Done 'NB_ITERS' times
//
// VARIABLES:
// - NB_ITERS
// - DB_NBS

//fwd declaration
ocrGuid_t driverEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);

ocrGuid_t edtCode(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    // Timer is the last DB
    timestamp_t * timersPtr = (timestamp_t *) depv[DB_NBS].ptr;
    long timerUsElapsed = elapsed_usec(&timersPtr[0], &timersPtr[1]);

    u64 driverIteration = paramv[0];
    long timerUsAcc = (long) paramv[1];
    timerUsAcc += timerUsElapsed;
    ocrGuid_t dbAllGuids = {.guid=paramv[2]};

    ocrGuid_t dbTimerGuid = depv[DB_NBS].guid;
    ocrDbRelease(dbTimerGuid);

    // Spawn the next driver iteration
    ocrGuid_t edtDriverTemplateGuid;
    ocrEdtTemplateCreate(&edtDriverTemplateGuid, driverEdt, 2, 2);
    u64 paramvDriverEdt[2];
    paramvDriverEdt[0] = driverIteration+1;
    paramvDriverEdt[1] = (u64) timerUsAcc;
    ocrGuid_t edtDriverGuid;
    ocrEdtCreate(&edtDriverGuid, edtDriverTemplateGuid,
                 2, paramvDriverEdt, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(dbAllGuids, edtDriverGuid, 0, DB_MODE_CONST);
    ocrAddDependence(dbTimerGuid, edtDriverGuid, 1, DB_MODE_RW);
    ocrEdtTemplateDestroy(edtDriverTemplateGuid);
    return NULL_GUID;
}

// Two paramv: [driverIteration,timerUsAcc]
// Two depc  : [dbAllGuids,dbTimer]
ocrGuid_t driverEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 driverIteration = paramv[0];
    long timerUsAcc = (long) paramv[1];
    ocrGuid_t dbAllGuids = depv[0].guid;
    ocrGuid_t * dbGuids = (ocrGuid_t *) depv[0].ptr;
    ocrGuid_t dbTimerGuid = depv[1].guid;
    timestamp_t * timersPtr = (timestamp_t *) depv[1].ptr;

    if (driverIteration == NB_ITERS) {
        ocrDbRelease(dbAllGuids);
        ocrDbRelease(dbTimerGuid);
        summary_throughput_dbl(usec_to_sec(timerUsAcc), NB_ITERS*DB_NBS);
        ocrShutdown();
    } else {
        ocrGuid_t edtCodeTemplateGuid;
        // Has DB_NBS dependences + the DB containing the timer
        ocrEdtTemplateCreate(&edtCodeTemplateGuid, edtCode, 3, DB_NBS+1);

        // Prepare the next edt code
        u64 paramvEdtCode[3];
        paramvEdtCode[0] = driverIteration;
        paramvEdtCode[1] = (u64) timerUsAcc;
        paramvEdtCode[2] = (u64) dbAllGuids.guid;

        ocrGuid_t edtGuid;
        ocrEdtCreate(&edtGuid, edtCodeTemplateGuid,
                     3, paramvEdtCode, DB_NBS+1, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        ocrEdtTemplateDestroy(edtCodeTemplateGuid);

        // Cannot spawn the next iteration here becaues we can create a deadlock if the edtCode
        // and edtDriver both acquire the DB in ITW.

        // Time and add dependences
        get_time(&timersPtr[0]);
        int i = 0;
        while (i < DB_NBS) {
            ocrAddDependence(dbGuids[i], edtGuid, i, DB_MODE_CONST);
            i++;
        }
        get_time(&timersPtr[1]);
        ocrDbRelease(dbTimerGuid);

        // Add timer dependence
        ocrAddDependence(dbTimerGuid, edtGuid, DB_NBS, DB_MODE_CONST);
    }

    return NULL_GUID;
}


ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    ocrGuid_t * dbAllPtr;
    ocrGuid_t dbAllGuids;
    ocrDbCreate(&dbAllGuids, (void **)&dbAllPtr, (sizeof(ocrGuid_t)*DB_NBS), 0, NULL_HINT, NO_ALLOC);

    // Create all DBs and record their guid in dbAllPtr
    int i = 0;
    while (i < DB_NBS) {
        DB_TYPE * dbPtr;
        ocrDbCreate(&dbAllPtr[i], (void **)&dbPtr, (sizeof(DB_TYPE)*DB_NB_ELT), 0, NULL_HINT, NO_ALLOC);
        // Doesn't really matter to fill the DB
        // Make sure the DB has been released for measurements purpose
        ocrDbRelease(dbAllPtr[i]);
        i++;
    }
    ocrDbRelease(dbAllGuids);

    timestamp_t * dbTimerPtr;
    ocrGuid_t dbTimerGuid;
    ocrDbCreate(&dbTimerGuid, (void **)&dbTimerPtr, sizeof(timestamp_t)*2, 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbTimerGuid);

    ocrGuid_t edtDriverTemplateGuid;
    ocrEdtTemplateCreate(&edtDriverTemplateGuid, driverEdt, 2, 2);

    u64 paramvDriverEdt[2];
    paramvDriverEdt[0] = 0;       //driverIteration
    paramvDriverEdt[1] = (u64) 0; //timerUsAcc
    ocrGuid_t driverEdtGuid;
    ocrEdtCreate(&driverEdtGuid, edtDriverTemplateGuid,
                 2, paramvDriverEdt, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
    ocrAddDependence(dbAllGuids, driverEdtGuid, 0, DB_MODE_CONST);
    ocrAddDependence(dbTimerGuid, driverEdtGuid, 1, DB_MODE_RW);
    ocrEdtTemplateDestroy(edtDriverTemplateGuid);

    return NULL_GUID;
}
