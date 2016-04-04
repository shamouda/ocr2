
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - Competing DB writer EDTs in ITW mode (with early dbRelease)
 */

#define TYPE_ELEM_DB int

// BIG
#define NB_ELEM_DB 2000
#define NB_WRITERS 40

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int chunkSize = (NB_ELEM_DB/NB_WRITERS);
    TYPE_ELEM_DB * data = (TYPE_ELEM_DB *) depv[0].ptr;
    // First check all DBs elements are equal
    u32 b = 0;
    while (b < NB_WRITERS) {
        u32 i = 0;
        while (i < chunkSize) {
            ASSERT (data[b*chunkSize] == data[(b*chunkSize)+i]);
            i++;
        }
        b++;
    }
    PRINTF("Calling release\n");
    ocrDbRelease(depv[0].guid);
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t eWriteEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    TYPE_ELEM_DB * data = (TYPE_ELEM_DB *) depv[0].ptr;
    // Then write a new value
    TYPE_ELEM_DB v = (TYPE_ELEM_DB) paramv[0];
    int chunkSize = (NB_ELEM_DB/NB_WRITERS);
    u32 i = 0;
    while (i < chunkSize) {
        data[(v*chunkSize)+i] = v;
        i++;
    }
    ocrDbRelease(depv[0].guid);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t affinity = affinities[0];
    ocrHint_t dbHint;
    ocrHintInit( &dbHint, OCR_HINT_DB_T );
    ocrSetHintValue( & dbHint, OCR_HINT_DB_AFFINITY, ocrAffinityToHintValue( affinity) );

    // Create DB @ affinity
    void * dbPtr;
    ocrGuid_t dbGuid;
    u32 nbElem = NB_ELEM_DB;
    ocrDbCreate(&dbGuid, &dbPtr, sizeof(TYPE_ELEM_DB) * NB_ELEM_DB, 0, &dbHint, NO_ALLOC);
    u64 i = 0;
    int * data = (int *) dbPtr;
    while (i < nbElem) {
        data[i] = -1;
        i++;
    }
    ocrDbRelease(dbGuid);

    ocrGuid_t shutdownEdtTemplateGuid;
    ocrEdtTemplateCreate(&shutdownEdtTemplateGuid, shutdownEdt, 0, NB_WRITERS+1);
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinity) );

    // shutdown EDT will check the DB content
    ocrGuid_t shutdownGuid;
    ocrEdtCreate(&shutdownGuid, shutdownEdtTemplateGuid, 0, NULL, NB_WRITERS+1, NULL,
                 EDT_PROP_NONE, &edtHint, NULL);
    ocrAddDependence(dbGuid, shutdownGuid, 0, DB_MODE_CONST);
    // ocrAddDependence(NULL_GUID, shutdownGuid, 0, DB_MODE_CONST);

    // create writer EDT for the DB in ITW mode @ affinity
    ocrGuid_t writerEdtTemplateGuid;
    ocrEdtTemplateCreate(&writerEdtTemplateGuid, eWriteEdt, 1, 1);

    i = 0;
    while (i < NB_WRITERS) {
        ocrGuid_t outputEventGuid;
        ocrGuid_t writerEdtGuid;
        ocrHintInit( &edtHint, OCR_HINT_EDT_T );
        ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affinities[i % affinityCount]) );
        ocrEdtCreate(&writerEdtGuid, writerEdtTemplateGuid, 1, &i, 1, NULL,
                     EDT_PROP_NONE, &edtHint, &outputEventGuid);
        ocrAddDependence(outputEventGuid, shutdownGuid, i+1, DB_MODE_CONST);
        ocrAddDependence(dbGuid, writerEdtGuid, 0, DB_MODE_RW);
        // ocrAddDependence(NULL_GUID, writerEdtGuid, 0, DB_MODE_RW);
        i++;
    }

    return NULL_GUID;
}
