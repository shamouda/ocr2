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
    timestamp_t * timers = (timestamp_t *) depv[1].ptr;
    get_time(&timers[1]);
    summary_throughput_timer(&timers[0], &timers[1], NB_INSTANCES);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    return NULL_GUID;
}

ocrGuid_t headEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtTemplateGuid;
    timestamp_t * dbPtr = depv[0].ptr;
    ocrGuid_t dbGuid = depv[0].guid;

    get_time(&dbPtr[0]);
    ocrDbRelease(dbGuid);

    ocrGuid_t workEdtTemplateGuid;
    ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, 0, 0);

    int i = 0;
    while (i < NB_INSTANCES) {
        ocrGuid_t workEdtGuid;
        ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                     0, NULL, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        i++;
    }
    ocrEdtTemplateDestroy(workEdtTemplateGuid);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0, 2);
    ocrGuid_t terminateEdtGuid;
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid,
                 0, NULL, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    timestamp_t * dbPtr;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid, (void **)&dbPtr, (sizeof(timestamp_t)*2), 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbGuid);

    ocrGuid_t headEdtGuidTemplateGuid;
    ocrEdtTemplateCreate(&headEdtGuidTemplateGuid, headEdt, 0, 1);
    ocrGuid_t headEdtGuid;
    ocrGuid_t outEvent;
    ocrEdtCreate(&headEdtGuid, headEdtGuidTemplateGuid,
                 0, NULL, 1, NULL, EDT_PROP_FINISH, NULL_HINT, &outEvent);

    ocrAddDependence(outEvent, terminateEdtGuid, 0, DB_MODE_CONST);
    ocrAddDependence(dbGuid, terminateEdtGuid, 1, DB_MODE_CONST);

    ocrAddDependence(dbGuid, headEdtGuid, 0, DB_MODE_CONST);
    return NULL_GUID;
}
