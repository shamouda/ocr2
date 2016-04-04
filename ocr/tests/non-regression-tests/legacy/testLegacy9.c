/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

// Only tested when OCR library interface is available
#ifdef ENABLE_EXTENSION_LEGACY

#include "extensions/ocr-affinity.h"
#include "extensions/ocr-legacy.h"

/**
 * DESC: Test reading the DB returned by ocrLegacyBlockProgress (BUG #840)
 *       EDT and DB not to beaffinitized with one another to force
 *       ocrLegacyBlockProgress to perform a blocking remote acquire
 */

#define MARK 999

// This is the "key" EDT that is responsible for spawning all the workers
ocrGuid_t keyEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrHint_t dbHint;
    ocrHintInit( &dbHint, OCR_HINT_DB_T );
    ocrSetHintValue( & dbHint, OCR_HINT_DB_AFFINITY, ocrAffinityToHintValue( affinities[affinityCount-1]) );

    u64 * array;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid,(void **) &array, sizeof(u64), 0, &dbHint, NO_ALLOC);
    array[0] = MARK;
    ocrDbRelease(dbGuid);
    return dbGuid;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t template;
    ocrEdtTemplateCreate(&template, keyEdt, 0, 1);

    ocrGuid_t stickyEvtGuid;
    ocrEventCreate(&stickyEvtGuid, OCR_EVENT_STICKY_T, EVT_PROP_TAKES_ARG);
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);

    // Make sure the EDT and DBs are not affinitized with one another
    ocrGuid_t edtGuid, outputEventGuid;
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinities[0]) );
    ocrEdtCreate(&edtGuid, template, 0, NULL,
                 1, NULL, EDT_PROP_NONE, &edtHint, &outputEventGuid);
    ocrAddDependence(outputEventGuid, stickyEvtGuid, 0, DB_DEFAULT_MODE);

    // Schedule the EDT
    ocrAddDependence(NULL_GUID, edtGuid, 0, DB_DEFAULT_MODE);

    ocrGuid_t dbGuid;
    void * result;
    u64 size;
    ocrLegacyBlockProgress(stickyEvtGuid, &dbGuid, &result, &size, LEGACY_PROP_NONE);
    ASSERT(!(ocrGuidIsNull(dbGuid)));
    ASSERT(result != NULL);
    ASSERT(((u64 *) result)[0] == MARK);
    ASSERT(size == sizeof(u64));

    ocrShutdown();
    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
