/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#include "extensions/ocr-resiliency-sim.h"
#include "resiliency/null/null-resiliency.h"


#include "ocr-resiliency.h"

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "policy-domain/hc/hc-policy.h"

#include "ocr-sal.h"


u8 ocrInjectFault(u64 newFreq){
    PRINTF("\nINJECTING FAULT CODE\n\n");

    ocrPolicyDomain_t *pd;
    ocrWorker_t *worker;
    getCurrentEnv(&pd, &worker, NULL, NULL);
    ocrPolicyDomainHc_t *rself = (ocrPolicyDomainHc_t *)pd;

    bool leaderFlag = false;

    if(hal_cmpswap32((u32 *)&rself->pqrFlags.pausingWorker, -1, worker->id) != -1 ||
        hal_cmpswap32((u32*)&rself->pqrFlags.runtimePause, false, true) == true){
        while(hal_cmpswap32((u32*)&rself->pqrFlags.runtimePause, false, true) == true){
            if(rself->pqrFlags.runtimePause == true){
                hal_xadd32((u32*)&rself->pqrFlags.pauseCounter, 1);
                while(rself->pqrFlags.runtimePause == true){
                    hal_pause();
                }
                hal_xadd32((u32*)&rself->pqrFlags.pauseCounter, -1);
            }
        }
    }else{
        leaderFlag = true;
    }

    hal_xadd32((u32*)&rself->pqrFlags.pauseCounter, 1);

    //Ensure rest of workers have reached queiescence
    while(rself->pqrFlags.pauseCounter < pd->workerCount){
        hal_pause();
    }

    //Worker responsible for injecting fault
    if(leaderFlag == true){
        //Arrange recover() parameters
        ocrResiliency_t *self = pd->resiliency[0];
        ocrFaultCode_t faultCode = OCR_FAULT_FREQUENCY;
        ocrLocation_t loc = (ocrLocation_t) rself->pqrFlags.pausingWorker;
        u64 *buffer = malloc(sizeof(u64));
        buffer[0] = newFreq;

        u32 returnCode = self->fcts.recover(self, faultCode, loc, buffer);
        ASSERT(returnCode == 0);

    }

    return 0;
}


