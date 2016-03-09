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

#define NB_SALVO 10
#define PROD_SALVO 2

ocrGuid_t shtEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

ocrGuid_t prodEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t evtToConsGuid = {.guid=paramv[0]};
    ocrGuid_t evtToProdGuid = {.guid=paramv[1]};
    u64 countSalvo = paramv[2];
    u32 i = 0;
    while (i < PROD_SALVO) {
        ocrGuid_t dbGuid;
        u32 * dbPtr;
        ocrDbCreate(&dbGuid, (void **)&dbPtr, sizeof(u32), 0, NULL_HINT, NO_ALLOC);
        dbPtr[0] = (countSalvo * PROD_SALVO) + i;
        PRINTF("Send=%"PRId32" val=%"PRId32" guid="GUIDF" @ salvo=%"PRId32"\n", i, dbPtr[0], GUIDA(dbGuid), countSalvo);
        ocrDbRelease(dbGuid);
        ocrEventSatisfy(evtToConsGuid, dbGuid);
        i++;
    }
    countSalvo++;
    if (countSalvo < NB_SALVO) {
        ocrGuid_t affGuid;
        ocrAffinityGetCurrent(&affGuid);
        // Register next instance on the edt
        // Clone itself and register on the producer event
        ocrGuid_t prodTpl;
        ocrEdtTemplateCreate(&prodTpl, prodEdt, 3, 1);
        u64 pparamv[3];
        pparamv[0] = (u64) evtToConsGuid.guid;
        pparamv[1] = (u64) evtToProdGuid.guid;
        pparamv[2] = (u64) countSalvo;
        ocrGuid_t edtProdGuid;
        ocrHint_t edtHint;
        ocrHintInit( &edtHint, OCR_HINT_EDT_T );
        ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affGuid ) );
        ocrEdtCreate(&edtProdGuid, prodTpl, EDT_PARAM_DEF, pparamv,
                     EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);
        ocrAddDependence(evtToProdGuid, edtProdGuid, 0, DB_MODE_RO);
        ocrEdtTemplateDestroy(prodTpl);
    } // else just die out and the consumers will shutdown the runtime
    return NULL_GUID;
}

ocrGuid_t consEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t evtToConsGuid;
    evtToConsGuid.guid = paramv[0];
    ocrGuid_t evtToProdGuid;
    evtToProdGuid.guid = paramv[1];
    u64 countProd = paramv[2];
    u64 countSalvo = paramv[3];
    u64 expVal = paramv[4];
    u32 * dbPtr = (u32 *) depv[0].ptr;
    countProd++;
    PRINTF("Recv val=%"PRId32" guid="GUIDF" countProd=%"PRId32" @ salvo=%"PRId32"\n", dbPtr[0], GUIDA(depv[0].guid), countProd, countSalvo);
    ASSERT(expVal == *dbPtr);
    // Time to
    if (countProd == PROD_SALVO) {
        countSalvo++;
        if (countSalvo == NB_SALVO) {
            // We're done
            PRINTF("ocrShutdown Called\n");
            ocrShutdown();
            return NULL_GUID;
        } else {
            // Done with a salvo

            // Acknowledge the producer (which will start a new salvo)
            ocrEventSatisfy(evtToProdGuid, NULL_GUID);

            // Fall-through and spawn ourselves again for the next salvo
            countProd = 0;
        }
    }
    // Register next instance on the edt
    // Clone itself and register on the consumer event
    ocrGuid_t affGuid;
    ocrAffinityGetCurrent(&affGuid);
    ocrGuid_t consTpl;
    ocrEdtTemplateCreate(&consTpl, consEdt, 5, 1);
    u64 pparamv[5];
    pparamv[0] = (u64) evtToConsGuid.guid;
    pparamv[1] = (u64) evtToProdGuid.guid;
    pparamv[2] = (u64) countProd;
    pparamv[3] = (u64) countSalvo;
    pparamv[4] = (u64) expVal+1;
    ocrGuid_t edtConsGuid;
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( affGuid ) );
    ocrEdtCreate(&edtConsGuid, consTpl, EDT_PARAM_DEF, pparamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);
    ocrAddDependence(evtToConsGuid, edtConsGuid, 0, DB_MODE_RO);
    ocrEdtTemplateDestroy(consTpl);
    return NULL_GUID;
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrEventParams_t params;
    params.EVENT_CHANNEL.maxGen = PROD_SALVO;
    params.EVENT_CHANNEL.nbSat = 1;
    params.EVENT_CHANNEL.nbDeps = 1;
    ocrGuid_t evtToConsGuid;
    ocrEventCreateParams(&evtToConsGuid, OCR_EVENT_CHANNEL_T, false, &params);

    params.EVENT_CHANNEL.maxGen = 1;
    ocrGuid_t evtToProdGuid;
    ocrEventCreateParams(&evtToProdGuid, OCR_EVENT_CHANNEL_T, false, &params);

    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ASSERT(affinityCount >= 1);
    ocrGuid_t affinities[affinityCount];
    ocrAffinityGet(AFFINITY_PD, &affinityCount, affinities);
    ocrGuid_t prodAff = affinities[affinityCount-1];
    ocrGuid_t condAff = affinities[0];

    ocrGuid_t prodTpl;
    ocrEdtTemplateCreate(&prodTpl, prodEdt, 3, 1);
    u64 pparamv[3];
    pparamv[0] = (u64) evtToConsGuid.guid;
    pparamv[1] = (u64) evtToProdGuid.guid;
    pparamv[2] = 0;
    ocrGuid_t edtProdGuid;
    ocrHint_t edtHint;
    ocrHintInit( &edtHint, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( prodAff ) );
    ocrEdtCreate(&edtProdGuid, prodTpl, EDT_PARAM_DEF, pparamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint, NULL);

    ocrGuid_t consTpl;
    ocrEdtTemplateCreate(&consTpl, consEdt, 5, 1);
    u64 cparamv[5];
    cparamv[0] = (u64) evtToConsGuid.guid;
    cparamv[1] = (u64) evtToProdGuid.guid;
    cparamv[2] = 0;
    cparamv[3] = 0;
    cparamv[4] = 0; //expected value
    ocrGuid_t edtConsGuid;
    ocrHint_t edtHint2;
    ocrHintInit( &edtHint2, OCR_HINT_EDT_T );
    ocrSetHintValue( & edtHint2, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue( condAff ) );

    ocrEdtCreate(&edtConsGuid, consTpl, EDT_PARAM_DEF, cparamv,
                 EDT_PARAM_DEF, NULL, EDT_PROP_NONE, &edtHint2, NULL);

    ocrAddDependence(evtToConsGuid, edtConsGuid, 0, DB_MODE_RO);
    ocrAddDependence(NULL_GUID, edtProdGuid, 0, DB_MODE_RO);

    return NULL_GUID;
}

#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("Test disabled - ENABLE_EXTENSION_CHANNEL_EVT not defined\n");
    ocrShutdown();
    return NULL_GUID;
}

#endif
