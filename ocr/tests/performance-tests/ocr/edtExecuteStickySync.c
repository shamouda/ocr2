#include "perfs.h"
#include "ocr.h"

// DESC: One worker creates all the tasks. Sink EDT depends on
//       all tasks through individual sticky events.
// TIME: Completion of all tasks
// FREQ: Create 'NB_INSTANCES' EDTs once
//
// VARIABLES:
// - NB_INSTANCES

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    timestamp_t * timers = (timestamp_t *) depv[NB_INSTANCES].ptr;
    get_time(&timers[1]);
    summary_throughput_timer(&timers[0], &timers[1], NB_INSTANCES);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t eventGuid = *((ocrGuid_t *) paramv);
    ocrEventSatisfy(eventGuid, NULL_GUID);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtTemplateGuid;
    // Nb of tasks events to synchronize + timer DB
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0, NB_INSTANCES+1);
    ocrGuid_t * dbEvtPtr;
    ocrGuid_t dbEvtGuid;
    ocrDbCreate(&dbEvtGuid, (void **)&dbEvtPtr, (sizeof(ocrGuid_t)*NB_INSTANCES), 0, NULL_HINT, NO_ALLOC);

    ocrGuid_t terminateEdtGuid;
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid,
                 0, NULL, NB_INSTANCES+1, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    int k = 0;
    while (k < NB_INSTANCES) {
        ocrEventCreate(&dbEvtPtr[k], OCR_EVENT_STICKY_T, false);
        ocrAddDependence(dbEvtPtr[k], terminateEdtGuid, k, DB_MODE_CONST);
        k++;
    }

    timestamp_t * dbPtr;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid, (void **)&dbPtr, (sizeof(timestamp_t)*2), 0, NULL_HINT, NO_ALLOC);

    get_time(&dbPtr[0]);

    ocrGuid_t workEdtTemplateGuid;
    ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, 1, 0);

    int i = 0;
    while (i < NB_INSTANCES) {
        ocrGuid_t workEdtGuid;
        ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                     1, (u64 *) &dbEvtPtr[i], 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        i++;
    }
    ocrEdtTemplateDestroy(workEdtTemplateGuid);

    ocrDbRelease(dbEvtGuid);
    ocrDbRelease(dbGuid);
    ocrAddDependence(dbGuid, terminateEdtGuid, NB_INSTANCES, DB_MODE_CONST);
    return NULL_GUID;
}
