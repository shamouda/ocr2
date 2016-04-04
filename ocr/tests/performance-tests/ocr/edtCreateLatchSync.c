#include "perfs.h"
#include "ocr.h"

// DESC: One worker creates all the tasks. Sink EDT depends on
//       a latch event all tasks satisfy on decrement slot.
// TIME: Creation of all tasks
// FREQ: Create 'NB_INSTANCES' EDTs once
//
// VARIABLES:
// - NB_INSTANCES

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    timestamp_t * timers = (timestamp_t *) depv[1].ptr;
    summary_throughput_timer(&timers[0], &timers[1], NB_INSTANCES);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t evLchGuid = *((ocrGuid_t *) paramv);
    ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t terminateEdtTemplateGuid;
    // Latch events to synchronize + timer DB
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0, 2);

    ocrGuid_t terminateEdtGuid;
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid,
                 0, NULL, 2, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    ocrGuid_t evLchGuid;
    ocrEventCreate(&evLchGuid, OCR_EVENT_LATCH_T, false);
    ocrAddDependence(evLchGuid, terminateEdtGuid, 0, DB_MODE_CONST);

    u32 k = 0;
    while (k < (NB_INSTANCES+1)) {
        // incr for number of edt + mainEdt
        ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_INCR_SLOT);
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
                     1, (u64 *) &evLchGuid, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        i++;
    }
    ocrEdtTemplateDestroy(workEdtTemplateGuid);

    get_time(&dbPtr[1]);
    ocrDbRelease(dbGuid);
    ocrAddDependence(dbGuid, terminateEdtGuid, 1, DB_MODE_CONST);

    ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    return NULL_GUID;
}
