#include "perfs.h"
#include "ocr.h"

// DESC: Create NB_WORKERS seed tasks that are responsible for doing work
//       and spaw their follow up task (i+NB_WORKERS)
// TIME: Execute all tasks
// FREQ: Create 'NB_INSTANCES' EDTs
//
// VARIABLES:
// - NB_INSTANCES
// - NB_WORKERS
// - SINGLE_TEMPLATE (not exposed)

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    timestamp_t * timers = (timestamp_t *) depv[1].ptr;
    get_time(&timers[1]);
    summary_throughput_timer(&timers[0], &timers[1], NB_INSTANCES);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 idx = paramv[0];
    u64 ub = paramv[1];
#ifdef SINGLE_TEMPLATE
    ocrGuid_t tpl = (ocrGuid_t) paramv[3];
#endif
    u64 next = idx + NB_WORKERS;
    if (next > ub) {
        ocrGuid_t evLchGuid;
        evLchGuid.guid = paramv[2];
        ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    } else {
#ifndef SINGLE_TEMPLATE
        ocrGuid_t workEdtTemplateGuid;
        ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, paramc, depc);
#else
#endif
        paramv[0] = next;
        ocrGuid_t workEdtGuid;
        ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                     EDT_PARAM_DEF, paramv, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
#ifndef SINGLE_TEMPLATE
        ocrEdtTemplateDestroy(workEdtTemplateGuid);
#endif
    }

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
    while (k < (NB_WORKERS+1)) {
        // incr for number of edt + mainEdt
        ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_INCR_SLOT);
        k++;
    }

    timestamp_t * dbPtr;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid, (void **)&dbPtr, (sizeof(timestamp_t)*2), 0, NULL_HINT, NO_ALLOC);

    get_time(&dbPtr[0]);

    ocrGuid_t workEdtTemplateGuid;
#ifdef SINGLE_TEMPLATE
    u32 nparamc = 4;
#else
    u32 nparamc = 3;
#endif
    ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, nparamc, 0);

    u32 i = 0;
    u64 nparamv[nparamc];
    nparamv[1] = NB_INSTANCES;
    nparamv[2] = (u64) evLchGuid.guid;
#ifdef SINGLE_TEMPLATE
    nparamv[3] = (u64) workEdtTemplateGuid;
#endif

    while (i < NB_WORKERS) {
        nparamv[0] = i;
        ocrGuid_t workEdtGuid;
        ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                     EDT_PARAM_DEF, nparamv, 0, NULL, EDT_PROP_NONE, NULL_HINT, NULL);
        i++;
    }
#ifndef SINGLE_TEMPLATE
    ocrEdtTemplateDestroy(workEdtTemplateGuid);
#endif

    ocrDbRelease(dbGuid);
    ocrAddDependence(dbGuid, terminateEdtGuid, 1, DB_MODE_CONST);

    ocrEventSatisfySlot(evLchGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    return NULL_GUID;
}
