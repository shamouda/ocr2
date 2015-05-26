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
        salPause(true);
        return;
    }

    if(sigNum == SIGUSR1 && globalPD->runtimePause == true){
        PRINTF("Resuming Runtime\n");
        salResume(1);
    }

    if(sigNum == SIGUSR2 && globalPD->runtimePause == true){
        //TODO: Support this with new querying methodology?
        PRINTF("\nQuery Not Supported via signalling\n");
        //salQuery(1, OCR_QUERY_WORKPILE_EDTS, NULL_GUID, &sigRes, &sigSize, 0);
        return;
    }

    if(sigNum == SIGUSR2 && globalPD->runtimePause == false){
        PRINTF("Nothing to do\n");
        return;
    }

}

u32 salPause(bool isBlocking){

    ocrPolicyDomain_t *pd;
    ocrWorker_t *baseWorker;
    getCurrentEnv(&pd, &baseWorker, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;

    while(hal_cmpswap32((u32*)&self->runtimePause, false, true) == true) {
        // Already paused - try to pause self only if blocked
        if(isBlocking == false)
            return 0;
        // Blocking pause
        if(self->runtimePause == true) {
            hal_xadd32((u32*)&self->pauseCounter, 1);
            //Pause called - stop workers
            while(self->runtimePause == true) {
                hal_pause();
            }
            hal_xadd32((u32*)&self->pauseCounter, -1);
        }
    }

    hal_xadd32((u32*)&self->pauseCounter, 1);

    while(self->pauseCounter < self->base.workerCount){
        hal_pause();
    }

    return 1;
}

ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){

    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;
    ocrGuid_t dataDb = NULL_GUID;

    if(self->runtimePause == false)
        return NULL_GUID;

    switch(query){
        case OCR_QUERY_WORKPILE_EDTS:
            dataDb = hcQueryNextEdts(self, result, size);
            *size = (*size)*sizeof(ocrGuid_t);
            break;

        case OCR_QUERY_EVENTS:
            //TODO: Call Query Event Function
            break;

        case OCR_QUERY_DATABLOCKS:
            //TODO: Query Datablock Function
            break;

        case OCR_QUERY_ALL_EDTS:
            //TODO: Query all Edts function
            break;

        default:
            break;
    }

    return dataDb;
}

void salResume(u32 flag){
    ocrPolicyDomain_t *pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrPolicyDomainHc_t *self = (ocrPolicyDomainHc_t *) pd;

    if(hal_cmpswap32((u32*)&self->runtimePause, true, false) == true)
        hal_xadd32((u32*)&self->pauseCounter, -1);

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
