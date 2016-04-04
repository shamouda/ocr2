/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

/* Example of a pattern that highlights the
 * expressiveness of task dependences
 *
 * Implements the following dependence graph:
 *
 * mainEdt
 * |      \
 * stage1a stage1b
 * |     \       |
 * |      \      |
 * |       \     |
 * stage2a  stage2b
 *     \      /
 *     shutdownEdt
 */
#include "ocr.h"

#define NB_ELEM_DB 20

typedef struct {
    ocrGuid_t evtGuid;
} guidPRM_t;

// How many parameters does it take to encode a GUID
#define PARAM_SIZE (sizeof(guidPRM_t) + sizeof(u64) - 1)/sizeof(u64)

ocrGuid_t shutdownEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 2);
    u64* data0 = (u64*)depv[0].ptr;
    u64* data1 = (u64*)depv[1].ptr;

    ASSERT(*data0 == 3ULL);
    ASSERT(*data1 == 4ULL);
    PRINTF("Got a DB (GUID "GUIDF") containing %"PRId32" on slot 0\n", GUIDA(depv[0].guid), *data0);
    PRINTF("Got a DB (GUID "GUIDF") containing %"PRId32" on slot 1\n", GUIDA(depv[1].guid), *data1);

    // Free the data blocks that were passed in
    ocrDbDestroy(depv[0].guid);
    ocrDbDestroy(depv[1].guid);

    // Shutdown the runtime
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t stage2a(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);

ocrGuid_t stage1a(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 1);
    ASSERT(paramc == PARAM_SIZE);
    // paramv contains the event that the child EDT has to satisfy
    // when it is done

    // We create a data block for one u64 and put data in it
    ocrGuid_t dbGuid = NULL_GUID, stage2aTemplateGuid = NULL_GUID,
        stage2aEdtGuid = NULL_GUID;
    u64* dbPtr = NULL;
    ocrDbCreate(&dbGuid, (void**)&dbPtr, sizeof(u64), 0, NULL_HINT, NO_ALLOC);
    *dbPtr = 1ULL;

    // Create an EDT and pass it the data block we just created
    // The EDT is immediately ready to execute
    ocrEdtTemplateCreate(&stage2aTemplateGuid, stage2a, PARAM_SIZE, 1);
    ocrEdtCreate(&stage2aEdtGuid, stage2aTemplateGuid, EDT_PARAM_DEF,
                 paramv, EDT_PARAM_DEF, &dbGuid, EDT_PROP_NONE, NULL_HINT, NULL);

    // Pass the same data block created to stage2b (links setup in mainEdt)
    return dbGuid;
}

ocrGuid_t stage1b(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 1);
    ASSERT(paramc == 0);

    // We create a data block for one u64 and put data in it
    ocrGuid_t dbGuid = NULL_GUID;
    u64* dbPtr = NULL;
    ocrDbCreate(&dbGuid, (void**)&dbPtr, sizeof(u64), 0, NULL_HINT, NO_ALLOC);
    *dbPtr = 2ULL;

    // Pass the created data block created to stage2b (links setup in mainEdt)
    return dbGuid;
}

ocrGuid_t stage2a(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 1);
    ASSERT(paramc == PARAM_SIZE);

    guidPRM_t *params = (guidPRM_t*)paramv;

    u64 *dbPtr = (u64*)depv[0].ptr;
    ASSERT(*dbPtr == 1ULL); // We got this from stage1a

    *dbPtr = 3ULL; // Update the value

    // Pass the modified data block to shutdown
    ocrEventSatisfy(params->evtGuid, depv[0].guid);

    return NULL_GUID;
}

ocrGuid_t stage2b(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ASSERT(depc == 2);
    ASSERT(paramc == 0);

    u64 *dbPtr = (u64*)depv[1].ptr;
    // Here, we can run concurrently to stage2a which modifies the value
    // we see in depv[0].ptr. We should see either 1ULL or 3ULL

    // On depv[1], we get the value from stage1b and it should be 2
    ASSERT(*dbPtr == 2ULL); // We got this from stage2a

    *dbPtr = 4ULL; // Update the value

    return depv[1].guid; // Pass this to the shudown EDT
}


ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {

    // Create the shutdown EDT
    ocrGuid_t stage1aTemplateGuid = NULL_GUID, stage1bTemplateGuid = NULL_GUID,
        stage2aTemplateGuid = NULL_GUID, stage2bTemplateGuid = NULL_GUID,
        shutdownEdtTemplateGuid = NULL_GUID;
    ocrGuid_t shutdownEdtGuid = NULL_GUID, stage1aEdtGuid = NULL_GUID,
        stage1bEdtGuid = NULL_GUID, stage2bEdtGuid = NULL_GUID,
        evtGuid = NULL_GUID, stage1aOut = NULL_GUID, stage1bOut = NULL_GUID,
        stage2bOut = NULL_GUID;

    ocrEdtTemplateCreate(&shutdownEdtTemplateGuid, shutdownEdt, 0, 2);
    ocrEdtCreate(&shutdownEdtGuid, shutdownEdtTemplateGuid, 0, NULL, EDT_PARAM_DEF, NULL,
                 EDT_PROP_NONE, NULL_HINT, NULL);

    // Create the event to satisfy shutdownEdt by stage 2a
    // (stage 2a is created by 1a)
    ocrEventCreate(&evtGuid, OCR_EVENT_ONCE_T, true);

    guidPRM_t tmp;
    tmp.evtGuid = evtGuid;
    // Create stages 1a, 1b and 2b
    // For 1a and 1b, add a "fake" dependence to avoid races between
    // setting up the event links and running the EDT
    ocrEdtTemplateCreate(&stage1aTemplateGuid, stage1a, PARAM_SIZE, 1);
    ocrEdtCreate(&stage1aEdtGuid, stage1aTemplateGuid, EDT_PARAM_DEF, (u64*)(&tmp),
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, &stage1aOut);

    ocrEdtTemplateCreate(&stage1bTemplateGuid, stage1b, 0, 1);
    ocrEdtCreate(&stage1bEdtGuid, stage1bTemplateGuid, EDT_PARAM_DEF, NULL,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, &stage1bOut);

    ocrEdtTemplateCreate(&stage2bTemplateGuid, stage2b, 0, 2);
    ocrEdtCreate(&stage2bEdtGuid, stage2bTemplateGuid, EDT_PARAM_DEF, NULL,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, &stage2bOut);

    // Set up all the links
    // 1a -> 2b
    ocrAddDependence(stage1aOut, stage2bEdtGuid, 0, DB_DEFAULT_MODE);

    // 1b -> 2b
    ocrAddDependence(stage1bOut, stage2bEdtGuid, 1, DB_DEFAULT_MODE);

    // Event satisfied by 2a -> shutdown
    ocrAddDependence(evtGuid, shutdownEdtGuid, 0, DB_DEFAULT_MODE);
    // 2b -> shutdown
    ocrAddDependence(stage2bOut, shutdownEdtGuid, 1, DB_DEFAULT_MODE);

    // Start 1a and 1b
    ocrAddDependence(NULL_GUID, stage1aEdtGuid, 0, DB_DEFAULT_MODE);
    ocrAddDependence(NULL_GUID, stage1bEdtGuid, 0, DB_DEFAULT_MODE);

    return NULL_GUID;
}


