#include "perfs.h"
#include "ocr.h"
#include "extensions/ocr-affinity.h"

// DESC: One worker creates all the remote tasks. Sink EDT depends on a latch
//       event co-located with remote tasks.
// TIME: Completion of all tasks
// FREQ: Create 'NB_INSTANCES' EDTs once
//
// VARIABLES:
// - NB_INSTANCES

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // timestamp_t * timers = (timestamp_t *) depv[NB_INSTANCES].ptr;
    // get_time(&timers[1]);
    // summary_throughput_timer(&timers[0], &timers[1], NB_INSTANCES);
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t workEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t eventGuid = *((ocrGuid_t *) paramv);
    // struct timespec
    // timestamp_t req;
    // req.tv_nsec = 30000;
    // timestamp_t res;
    // nanosleep(&req, &res);
    ocrEventSatisfySlot(eventGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    return NULL_GUID;
}

ocrGuid_t headEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 count = 0;
    ocrAffinityCount(AFFINITY_PD, &count);
    ocrGuid_t affinities[count];
    ocrAffinityGet(AFFINITY_PD, &count, affinities);

    ocrGuid_t evtGuid = *((ocrGuid_t*)(depv[0].ptr));

    ocrGuid_t workEdtTemplateGuid;
    ocrEdtTemplateCreate(&workEdtTemplateGuid, workEdt, 1, 0);

    long timings[NB_INSTANCES];
    timestamp_t ts_start;
    timestamp_t ts_stop;

    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinities[count-1]) );

    // Spawn all of these on another PD
    int k = 0;
    while (k < NB_INSTANCES) {
        get_time(&ts_start);
        ocrGuid_t workEdtGuid;
        ocrEdtCreate(&workEdtGuid, workEdtTemplateGuid,
                     1, (u64 *) &evtGuid, 0, NULL, EDT_PROP_NONE, &edtHint, NULL);
        get_time(&ts_stop);
        timings[k] = elapsed_usec(&ts_start, &ts_stop);
        k++;
    }

    double avg = avg_usec(timings, NB_INSTANCES);
    printf("AVG per edt    (us): %f\n", avg);

    ocrEventSatisfySlot(evtGuid, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);

    return NULL_GUID;
}

ocrGuid_t lchCreatorEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t * guid = (ocrGuid_t *) depv[0].ptr;
    ocrGuid_t evGuid;
    ocrEventCreate(&evGuid, OCR_EVENT_LATCH_T, false);
    *guid = evGuid;
    ocrDbRelease(depv[0].guid);

    u32 i = 0;
    while (i < (NB_INSTANCES+1)) {
        ocrEventSatisfySlot(evGuid, NULL_GUID, OCR_EVENT_LATCH_INCR_SLOT);
        i++;
    }

    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0, 1);

    ocrGuid_t terminateEdtGuid;
    ocrEdtCreate(&terminateEdtGuid, terminateEdtTemplateGuid,
                 0, NULL, 1, &evGuid, EDT_PROP_NONE, NULL_HINT, NULL);


    ocrGuid_t headEdtTemplateGuid;
    ocrEdtTemplateCreate(&headEdtTemplateGuid, headEdt, 0, 1);

    ocrGuid_t curAffinity;
    ocrAffinityGetCurrent(&curAffinity);

    u64 count = 0;
    ocrAffinityCount(AFFINITY_PD, &count);
    ocrGuid_t affinities[count];
    ocrAffinityGet(AFFINITY_PD, &count, affinities);

    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinities[0] ));

    ocrGuid_t headEdtGuid;
    ocrEdtCreate(&headEdtGuid, headEdtTemplateGuid,
                 0, NULL, 1, &(depv[0].guid), EDT_PROP_NONE, &edtHint, NULL);

    return NULL_GUID;
}


ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 count = 0;
    ocrAffinityCount(AFFINITY_PD, &count);
    ocrGuid_t affinities[count];
    ocrAffinityGet(AFFINITY_PD, &count, affinities);

    ocrGuid_t curAffinity;
    ocrAffinityGetCurrent(&curAffinity);

    ASSERT(ocrGuidIsEq(curAffinity,affinities[0]));

    // Create DB containing to contain the guid latch
    ocrGuid_t * dbLchPtr;
    ocrGuid_t dbLchGuid;
    ocrDbCreate(&dbLchGuid, (void **)&dbLchPtr, sizeof(ocrGuid_t), 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbLchGuid);

    ocrGuid_t lchCreatorTemplateGuid;
    // Nb of tasks events to synchronize + timer DB
    ocrEdtTemplateCreate(&lchCreatorTemplateGuid, lchCreatorEdt, 0, 1);
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinities[count-1]) );

    ocrGuid_t lchCreatorEdtGuid;
    ocrEdtCreate(&lchCreatorEdtGuid, lchCreatorTemplateGuid,
                 0, NULL, 1, &dbLchGuid, EDT_PROP_NONE, &edtHint, NULL);
    return NULL_GUID;
}
