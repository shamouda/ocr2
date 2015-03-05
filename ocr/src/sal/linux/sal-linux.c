#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_HC

#include "ocr-hal.h"
#include "ocr-types.h"
#include <signal.h>
#include "policy-domain/hc/hc-policy.h"
#include "worker/hc/hc-worker.h"
#include "comp-platform/pthread/pthread-comp-platform.h"
#include "ocr-policy-domain.h"

/* NOTE: Below is an optional interface allowing users to
 *       send SIGUSR1 and SIGUSR2 to control pause/query/resume
 *       during execution.  By default the signaled pause command
 *       will block until it succeeds uncontended.
 *
 * SIGUSR1: Toggles Pause and Resume
 * SIGUSR2: Query the contents of queued tasks (will only
 *          succeed if runtime is paused)
 */
void sig_handler(u32 sigNum) {

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *globalPD = (ocrPolicyDomainHc_t *) pd;


    if(sigNum == SIGUSR1 && globalPD->runtimePause == false){
        PRINTF("Pausing Runtime\n");
        salPause();
        return;
    }

    if(sigNum == SIGUSR1 && globalPD->runtimePause == true){
        PRINTF("Resuming Runtime\n");
        salResume(1);
    }

    if(sigNum == SIGUSR2 && globalPD->runtimePause == true){
        PRINTF("\nDumping Worker Data...\n");
        salQuery(1);
        return;
    }

    if(sigNum == SIGUSR2 && globalPD->runtimePause == false){
        PRINTF("Nothing to do\n");
        return;
    }

}

u32 salPause(void){

    ocrPolicyDomain_t *pd;
    ocrWorker_t *baseWorker;
    getCurrentEnv(&pd, &baseWorker, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;
    ocrWorkerHc_t *wrkr = (ocrWorkerHc_t *) baseWorker;

    if(hal_cmpswap32((u32*)&self->pausingWorker, -1, wrkr->worker.seqId) != -1){
        return 0;
    }

    hal_cmpswap32((u32*)&self->runtimePause, false, true);

    PRINTF("\n\n\nPausing Runtime...\n\n\n");
    u32 i = 0;
    for(i = 0; i < self->base.workerCount; i++){
        //Query from here, check each of the worker's flags
        ocrWorkerHc_t *hcWorker = (ocrWorkerHc_t *) self->base.workers[i];

        #ifdef OCR_ENABLE_EDT_NAMING
            char * name = hcWorker->name;
        #else
            char * name = "OCR_EDT_NAMING not defined";
        #endif

        if(hcWorker->id == self->pausingWorker){
            PRINTF("Last EDT executed before pause on worker %lu - guid: 0x%llx template: 0x%llx fctPtr: %p name: %s (pausing worker)\n",
                    hcWorker->id, hcWorker->edtGuid, hcWorker->templateGuid, hcWorker->fctPtr, name);
        }else{
            PRINTF("Last EDT executed before pause on worker %lu - guid: 0x%llx template: 0x%llx fctPtr: %p name: %s\n",
                    hcWorker->id, hcWorker->edtGuid, hcWorker->templateGuid, hcWorker->fctPtr, name);
            //TODO:  This needs to change. It is assuming the runtime is fully paused.
            //workaround for now, but should be moved to worker level later.
            self->pauseCounter++;
        }
    }

    while(self->pauseCounter != self->base.workerCount-1){//-1){
        PRINTF("%d\n", self->pauseCounter);
        hal_pause();
    }

    return 1;
}

void salQuery(u32 flag){
    //Indicates pause was contended by this worker.  Do not perform query
    if(flag == 0){
        return;
    }
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;

    if(self->queryCounter == 0){
        hcDumpWorkerData(self);
    }

    while(self->queryCounter != self->base.workerCount){
        hal_pause();
    }
    return;
}

void salResume(u32 flag){
    //Indicates pause was contended by this worker.  Do not perform resume
    if(flag == 0){
        return;
    }

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;


    u32 oldQueryCounter = self->queryCounter;
    hal_cmpswap32((u32*)&self->queryCounter, oldQueryCounter, 0);

    u32 oldPauseCounter = self->pauseCounter;
    hal_cmpswap32((u32*)&self->pauseCounter, oldPauseCounter, 0);

    u32 oldPausingWorker = self->pausingWorker;
    hal_cmpswap32((u32*)&self->pausingWorker, oldPausingWorker, -1);

    bool oldRuntimePause = self->runtimePause;
    hal_cmpswap32((u32*)&self->runtimePause, oldRuntimePause, false);

    PRINTF("\n\nResuming Runtime...\n\n\n");
    return;

}

void registerSignalHandler(){

    struct sigaction action;
    action.sa_handler = ((void (*)(int))&sig_handler);
    action.sa_flags = SA_RESTART;
    sigfillset(&action.sa_mask);
    if(sigaction(SIGUSR1, &action, NULL) != 0) {
        PRINTF("Couldn't catch SIGUSR1...\n");
    }
     if(sigaction(SIGUSR2, &action, NULL) != 0) {
        PRINTF("Couldn't catch SIGUSR2...\n");
    }
}

#endif /* ENABLE_COMP_PLATFORM_PTHREAD */
