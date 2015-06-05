/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */



#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_HC

#include "ocr-types.h"
#include "ocr-db.h"
#include "ocr-hal.h"
#include "ocr-sal.h"
#include "ocr-policy-domain.h"
#include "comp-platform/pthread/pthread-comp-platform.h"

#include "policy-domain/hc/hc-policy.h"
#include "workpile/hc/hc-workpile.h"

//return guid of next EDT on specified worker's workpile
ocrGuid_t hcDumpNextEdt(ocrWorker_t *worker, u32 *size){
    ocrPolicyDomain_t *pd = worker->pd;
    ocrWorkpileHc_t *workpile = (ocrWorkpileHc_t *)pd->schedulers[0]->workpiles[worker->seqId];
    ocrTask_t *curTask = NULL;

    u32 head = (workpile->deque->head%INIT_DEQUE_CAPACITY);
    u32 tail = (workpile->deque->tail%INIT_DEQUE_CAPACITY);
    u32 workpileSize = tail-head;

    if(workpileSize > 0){

        PD_MSG_STACK(msg);
        getCurrentEnv(NULL, NULL, NULL, &msg);
        ocrFatGuid_t fguid;
        fguid.guid = (ocrGuid_t)workpile->deque->data[tail-1];
        fguid.metaDataPtr = NULL;

    #define PD_MSG (&msg)
    #define PD_TYPE PD_MSG_GUID_INFO
        msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid.guid) = fguid.guid;
        PD_MSG_FIELD_IO(guid.metaDataPtr) = fguid.metaDataPtr;
        PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP | KIND_GUIDPROP;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
        ocrGuidKind msgKind = PD_MSG_FIELD_O(kind);
        curTask = (ocrTask_t *)PD_MSG_FIELD_IO(guid.metaDataPtr);
    #undef PD_MSG
    #undef PD_TYPE

        if(msgKind != OCR_GUID_EDT){
            PRINTF("\nNon-EDT guid type return from GUID_INFO message - Kind: %d\n", msgKind);
            return NULL_GUID;

        }else if(curTask != NULL){
            //One queried task per worker (next EDT guid)
            *size = 1;
            return curTask->guid;
        }

    }else{
        //One queried task per worker (next EDT guid)
        *size = 1;

        return NULL_GUID;
    }
    return NULL_GUID;
}


ocrGuid_t hcQueryNextEdts(ocrPolicyDomainHc_t *rself, void **result, u32 *qSize){
    u64 i;
    *qSize = 0;
    ocrGuid_t *workpileGuids;
    ocrGuid_t dataDb;

    ocrDbCreate(&dataDb, (void **)&workpileGuids, sizeof(ocrGuid_t)*(rself->base.workerCount),
                0, NULL_GUID, NO_ALLOC);

    for(i = 0; i < rself->base.workerCount; i++){
        u32 wrkrSize;
        workpileGuids[i] = hcDumpNextEdt(rself->base.workers[i], &wrkrSize);
        *qSize = (*qSize) + wrkrSize;
    }

    *result = workpileGuids;
    return dataDb;
}


ocrGuid_t hcQueryAllEdts(ocrPolicyDomainHc_t *rself, void **result, u32 *qsize){
    ocrPolicyDomain_t  *pd = &rself->base;
    ocrWorkpileHc_t *workpile;
    u64 dataBlockSize = 0;
    u32 i = 0;


    //Get number of runnable EDTs to create DB of proper size.
    for(i = 0; i < rself->base.workerCount; i++){
        workpile = (ocrWorkpileHc_t *)pd->schedulers[0]->workpiles[i];
        u32 head = (workpile->deque->head%INIT_DEQUE_CAPACITY);
        u32 tail = (workpile->deque->tail%INIT_DEQUE_CAPACITY);
        u32 workpileSize = tail-head;

        if(workpileSize > 0){
            dataBlockSize += workpileSize;
        }
    }

    ocrGuid_t *workpileGuids;
    ocrGuid_t dataDb;
    u32 idxOffset = -1;
    ocrDbCreate(&dataDb, (void **)&workpileGuids, sizeof(ocrGuid_t)*(dataBlockSize),
                0, NULL_GUID, NO_ALLOC);

    //Populate datablock with workpile EDTs.
    for(i = 0; i < rself->base.workerCount; i++){
        workpile = (ocrWorkpileHc_t *)pd->schedulers[0]->workpiles[i];
        u32 head = (workpile->deque->head%INIT_DEQUE_CAPACITY);
        u32 tail = (workpile->deque->tail%INIT_DEQUE_CAPACITY);
        u32 workpileSize = tail-head;

        if(workpileSize > 0){
            u32 j;
            for(j = head; j < tail; j++){
                idxOffset++;
                PD_MSG_STACK(msg);
                getCurrentEnv(NULL, NULL, NULL, &msg);
                ocrFatGuid_t fguid;
                fguid.guid = (ocrGuid_t)workpile->deque->data[j];
                fguid.metaDataPtr = NULL;

            #define PD_MSG (&msg)
            #define PD_TYPE PD_MSG_GUID_INFO
                msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                PD_MSG_FIELD_IO(guid.guid) = fguid.guid;
                PD_MSG_FIELD_IO(guid.metaDataPtr) = fguid.metaDataPtr;
                PD_MSG_FIELD_I(properties) = RMETA_GUIDPROP | KIND_GUIDPROP;
                RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
                ocrGuidKind msgKind = PD_MSG_FIELD_O(kind);
                ocrTask_t *curTask = (ocrTask_t *)PD_MSG_FIELD_IO(guid.metaDataPtr);
            #undef PD_MSG
            #undef PD_TYPE

                if(msgKind != OCR_GUID_EDT){
                    workpileGuids[idxOffset] = NULL_GUID;
                    continue;
                }else if(curTask != NULL){
                    workpileGuids[idxOffset] = curTask->guid;
                }
            }
        }
    }

    *result = workpileGuids;
    *qsize = dataBlockSize;
    return dataDb;
}

ocrGuid_t hcQueryPreviousDatablock(ocrPolicyDomainHc_t *self, void **result, u32 *qSize){
    *qSize = 0;
    ocrGuid_t *prevDb;
    ocrGuid_t dataDb;

    ocrDbCreate(&dataDb, (void **)&prevDb, sizeof(ocrGuid_t), 0, NULL_GUID, NO_ALLOC);
    prevDb[0] = self->prevDb;

    *qSize = 1;
    *result = prevDb;
    return dataDb;
}

#endif /* ENABLE_POLICY_DOMAIN_HC */
