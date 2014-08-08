/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <stdio.h>
#include <assert.h>

#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - create a remote EDT
 */

ocrGuid_t remoteEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    printf("[remote] RemoteEdt: executing\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    assert(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t edtAffinity = affinities[affinityCount-1]; //TODO this implies we know current PD is '0'

    // create local edt that depends on the remote edt, the db is automatically cloned
    ocrGuid_t remoteEdtTemplateGuid;
    ocrEdtTemplateCreate(&remoteEdtTemplateGuid, remoteEdt, 0, 0);

    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, remoteEdtTemplateGuid, 0, NULL, 0, NULL_GUID,
        EDT_PROP_NONE, edtAffinity, NULL);

    return NULL_GUID;
}
