/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - test querying the affinity of a NULL_GUID
 */

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    u8 res = ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ASSERT(res == 0);
    u64 i = 0;
    while (i < affinityCount) {
        ocrGuid_t readAff;
        res = ocrAffinityGetAt(AFFINITY_PD, i, &readAff);
        ASSERT(res == 0);
        ASSERT(ocrGuidIsEq(readAff, affinities[i]));
        i++;
    }
    ocrShutdown();
    return NULL_GUID;
}
