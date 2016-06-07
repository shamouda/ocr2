/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: Channel-event: String of producers EDTs sending to consumers
 */

#ifdef ENABLE_EXTENSION_CHANNEL_EVT

#define NB 10

ocrGuid_t oth(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u32 i = 0;
    while(i < NB) {
        u32 * dbPtr = (u32 *) depv[i].ptr;
        ASSERT(*dbPtr == i);
        i++;
    }
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrEventParams_t params;
    params.EVENT_CHANNEL.maxGen = EVENT_CHANNEL_UNBOUNDED;
    params.EVENT_CHANNEL.nbSat = 1;
    params.EVENT_CHANNEL.nbDeps = 1;
    ocrGuid_t evtToConsGuid;
    ocrEventCreateParams(&evtToConsGuid, OCR_EVENT_CHANNEL_T, false, &params);
    ocrGuid_t othTpl;
    ocrEdtTemplateCreate(&othTpl, oth, 0, NB);
    ocrGuid_t othEdt;
    ocrEdtCreate(&othEdt, othTpl, EDT_PARAM_DEF, NULL,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, NULL_HINT, NULL);

    u32 i = 0;
    while(i < NB) {
        u32 * dbPtr;
        ocrGuid_t dbGuid;
        ocrDbCreate(&dbGuid, (void **)&dbPtr, sizeof(u32), 0, NULL_HINT, NO_ALLOC);
        dbPtr[0] = i;
        ocrDbRelease(dbGuid);
        ocrEventSatisfy(evtToConsGuid, dbGuid);
        i++;
    }

    i=0;
    while(i < NB) {
        ocrAddDependence(evtToConsGuid, othEdt, i, DB_MODE_RW);
        i++;
    }

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_CHANNEL_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
