/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr.h"
#include "extensions/ocr-affinity.h"

/**
 * DESC: OCR-DIST - Create an EDT with an affinity GUID obtained from another PD
 */

ocrGuid_t run(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[])
{
    PRINTF("run\n");
    ocrGuid_t my_affinity;
    ocrAffinityGetCurrent(&my_affinity);
    PRINTF("current affinity is " GUIDF "\n", GUIDA(my_affinity));
    if (paramc==0) return NULL_GUID;
    ocrGuid_t aff = *(ocrGuid_t*)paramv;
    PRINTF("Spawned affinity is " GUIDF "\n", GUIDA(aff));
    ocrGuid_t trun, w1;
    ocrEdtTemplateCreate(&trun,run,EDT_PARAM_UNK, EDT_PARAM_UNK);
    ocrHint_t h1;
    ocrHintInit(&h1, OCR_HINT_EDT_T);
    ocrSetHintValue(&h1, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(aff));
    ocrEdtCreate(&w1,trun,0,0,0,0,EDT_PROP_NONE,&h1,0);
    return NULL_GUID;
}

ocrGuid_t stop(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[])
{
    PRINTF("stop\n");
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    u64 aff_count;
    ocrAffinityCount(AFFINITY_PD, &aff_count);
    // ASSERT(aff_count >= 2);
    ocrGuid_t affs[aff_count];
    ocrAffinityGet(AFFINITY_PD, &aff_count, affs);

    ocrGuid_t w1,w1e,w2,w2e,done,trun,tstop;
    ocrEdtTemplateCreate(&trun,run,EDT_PARAM_UNK, EDT_PARAM_UNK);
    ocrEdtTemplateCreate(&tstop,stop,EDT_PARAM_UNK, EDT_PARAM_UNK);

    ocrHint_t h1;
    ocrHintInit(&h1, OCR_HINT_EDT_T);
    ocrSetHintValue(&h1, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affs[0]));
    ocrHint_t h2;
    ocrHintInit(&h2, OCR_HINT_EDT_T);
    ocrSetHintValue(&h2, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affs[aff_count-1]));

    ASSERT(sizeof(u64)==sizeof(ocrGuid_t));
    u64 params1[] = { *(u64*)&affs[aff_count-1]};
    u64 params2[] = { *(u64*)&affs[0]};
    ocrEdtCreate(&w1,trun,1,params1,1,0,EDT_PROP_FINISH,&h1,&w1e);
    ocrEdtCreate(&w2,trun,1,params2,1,0,EDT_PROP_FINISH,&h2,&w2e);
    ocrEdtCreate(&done,tstop,0,0,2,0,EDT_PROP_NONE,NULL_HINT,0);
    ocrAddDependence(w1e,done,0,DB_MODE_NULL);
    ocrAddDependence(w2e,done,1,DB_MODE_NULL);
    ocrAddDependence(NULL_GUID,w1,0,DB_MODE_NULL);
    ocrAddDependence(NULL_GUID,w2,0,DB_MODE_NULL);

    return NULL_GUID;
}
