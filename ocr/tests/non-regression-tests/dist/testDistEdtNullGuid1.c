#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Create a null-GUID EDT that has depv containing a sticky event.
 */

ocrGuid_t terminateEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Everything went OK\n");
    ocrShutdown(); // This is the last EDT to execute, terminate
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t edtAffinity = affinities[affinityCount-1]; //TODO this implies we know current PD is '0'

    ocrGuid_t event1Guid;
    ocrEventCreate(&event1Guid,OCR_EVENT_ONCE_T, EVT_PROP_NONE);

    ocrGuid_t terminateEdtTemplateGuid;
    ocrEdtTemplateCreate(&terminateEdtTemplateGuid, terminateEdt, 0 /*paramc*/, 1 /*depc*/);
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( edtAffinity) );

    ocrEdtCreate(NULL, terminateEdtTemplateGuid, 0, NULL, 1, &event1Guid,
                 /*properties=*/EDT_PROP_NONE, &edtHint, /*outEvent=*/ NULL);

    ocrEventSatisfy(event1Guid, NULL_GUID);
    return NULL_GUID;
}
