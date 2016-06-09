#include "perfs.h"
#include "ocr.h"
#include "extensions/ocr-affinity.h"

// DESC:
// TIME: Time calls to ocrAddDependence
// FREQ: Do NB_ITERS calls
//
// VARIABLES
// - NB_ITERS
// - FAN_OUT


#ifndef NB_ITERS
#define NB_ITERS 1
#endif

#ifndef FAN_OUT
#define FAN_OUT 2
#endif

#define ADDDEP_MODE DB_MODE_RO

#define EVENT_TYPE OCR_EVENT_STICKY_T

// divisible by 2 is nice
// 2^20
#ifndef NB_SATISFY
#define NB_SATISFY 1024
// #define NB_SATISFY 65536
// #define NB_SATISFY 1048576
#endif

#define THROUGHPUT_METRIC NB_SATISFY

//
// User Part
//

typedef struct {
    //TODO: there should be a sub struct that we extend so that the framework can do the setup
    ocrGuid_t self;
    // end common
    //
    ocrGuid_t userSetupDoneEvt;
    ocrGuid_t stopTimerEvt;
    ocrGuid_t remoteLatchEvent; // the remote event
} domainSetup_t;

typedef struct {
    ocrGuid_t self;
    timestamp_t startTimer;
    timestamp_t stopTimer;
} domainKernel_t;

ocrGuid_t nullEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);


ocrGuid_t stopTimerEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // depv[0] is doneEVt
    ocrGuid_t kernelDbGuid = depv[1].guid;
    domainKernel_t * kernelDbPtr = (domainKernel_t *) depv[1].ptr;
    // Stop timer
    get_time(&kernelDbPtr->stopTimer);
    ocrDbRelease(kernelDbGuid);
    // Nothing else to do: the output event is hooked to the userKernelDoneEvt.
    return kernelDbGuid;
}

ocrGuid_t remoteSetupUserEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dsetupGuid = depv[0].guid;
    domainSetup_t * dsetup = (domainSetup_t *) depv[0].ptr;

    // Create 'remote' (local to here) latch event
    ocrGuid_t evtGuid;
    ocrEventParams_t params;
    params.EVENT_LATCH.counter = NB_SATISFY;
    ocrEventCreateParams(&evtGuid, OCR_EVENT_LATCH_T, false, &params);
    dsetup->remoteLatchEvent = evtGuid;

    // Setup callback for when the latch event fires
    ocrAddDependence(evtGuid, dsetup->stopTimerEvt, 0, DB_MODE_NULL);

    ocrGuid_t userSetupDoneEvt = dsetup->userSetupDoneEvt;
    ocrDbRelease(dsetupGuid);

    // Global setup is done
    ocrEventSatisfy(userSetupDoneEvt, NULL_GUID);

    return NULL_GUID;
}

// Create an event at the current affinity and writes
// the GUID into the domainSetup data-structure.
void domainSetup(ocrGuid_t userSetupDoneEvt, domainSetup_t * dsetup) {
    // This is for the domain kernel to callback and stop the timer
    ocrGuid_t stopTimerEvt;
    ocrEventCreate(&stopTimerEvt, OCR_EVENT_ONCE_T, true);

    // Create an EDT at a remote affinity to:
    // - Create a remote latch event and initialize it
    // - Hook that event to the local event declared above
    // - write back the guid of the remote latch event into the setup DB

    // - userSetupDoneEvt: to be satisfied when setup is done
    // - stopTimerEvt: to be satisfied when domain kernel is done
    dsetup->userSetupDoneEvt = userSetupDoneEvt;
    dsetup->stopTimerEvt = stopTimerEvt;

    u64 affinityCount;
    ocrAffinityCount(AFFINITY_PD, &affinityCount);
    ocrGuid_t remoteAffGuid;
    ocrAffinityGetAt(AFFINITY_PD, affinityCount-1, &remoteAffGuid);
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(remoteAffGuid));
    ocrGuid_t edtTemplGuid;
    ocrEdtTemplateCreate(&edtTemplGuid, remoteSetupUserEdt, 0, 1);
    ocrGuid_t edtGuid;
    ocrEdtCreate(&edtGuid, edtTemplGuid,
                 0, NULL, 1, NULL, EDT_PROP_NONE, &edtHint, NULL);
    // EW addresses the race that the current caller owns the DB and we're
    // trying to start the remote setup EDT concurrently. Since we do not have
    // the caller event we can't setup a proper dependence and rely on EW instead.
    ocrAddDependence(dsetup->self, edtGuid, 0, DB_MODE_EW);
    ocrEdtTemplateDestroy(edtTemplGuid);
}

//TODO timer stuff is deprecated

// The kernel to invoke
void domainKernel(ocrGuid_t userKernelDoneEvt, domainSetup_t * setupDbPtr, timestamp_t * timer) {
    // Setup: DB to use to satisfy the domain kernel's done event
    ocrGuid_t curAffGuid;
    ocrAffinityGetCurrent(&curAffGuid);
    ocrHint_t dbHint;
    ocrHintInit(&dbHint, OCR_HINT_DB_T);
    ocrSetHintValue(&dbHint, OCR_HINT_DB_AFFINITY, ocrAffinityToHintValue(curAffGuid));
    //TODO why is this not created by the caller as for domainSetup ?
    ocrGuid_t kernelDbGuid;
    domainKernel_t * kernelDbPtr;
    ocrDbCreate(&kernelDbGuid, (void**) &kernelDbPtr, sizeof(domainKernel_t), 0, &dbHint, NO_ALLOC);
    kernelDbPtr->self = kernelDbGuid;

    // Kernel's core

    // Create callback EDT to depend on stop timer event triggered by remote
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(curAffGuid));
    ocrGuid_t stopTpl;
    ocrEdtTemplateCreate(&stopTpl, stopTimerEdt, 0, 2);
    ocrGuid_t stopEdt;
    ocrGuid_t stopEdtDone;
    ocrEdtCreate(&stopEdt, stopTpl,
                 0, NULL, 2, NULL, EDT_PROP_NONE, &edtHint, &stopEdtDone);
    // Stop timer will satisfy the user kernel done event
    ocrAddDependence(stopEdtDone, userKernelDoneEvt, 0, DB_MODE_NULL);
    ocrAddDependence(setupDbPtr->stopTimerEvt, stopEdt, 0, DB_MODE_NULL);
    ocrAddDependence(kernelDbPtr->self, stopEdt, 1, DB_MODE_EW);

    // Start timer
    ocrGuid_t remoteEvt = setupDbPtr->remoteLatchEvent;
    get_time(&kernelDbPtr->startTimer);
    u64 i;
    for(i=0; i<NB_SATISFY; i++) {
        ocrEventSatisfySlot(remoteEvt, NULL_GUID, OCR_EVENT_LATCH_DECR_SLOT);
    }
    // Note: Timer stops when the remote latch event got all the satisfy
    ocrDbRelease(kernelDbGuid);
}

void domainKernelCombine(domainSetup_t * setupDbPtr, domainKernel_t * kernelPtr, long * elapsed) {
    *elapsed = elapsed_usec(&kernelPtr->startTimer, &kernelPtr->stopTimer);
}

// // - Completion event to be satisfied when execution is done (paramv[0])
// ocrGuid_t nullEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
//     // PRINTF("nullEdt\n");
//     // Stop the timer right away since we measure the spawn to execution time.
//     timestamp_t stopTimer;
//     get_time(&stopTimer);

//     ocrGuid_t kernelEdtDoneEvt = (ocrGuid_t) paramv[0];

//     // Done with kernel
//     ocrGuid_t kernelDbGuid;
//     domainKernel_t * kernelDbPtr;
//     //TODO create that on current affinity ?
//     ocrDbCreate(&kernelDbGuid, (void**) &kernelDbPtr, sizeof(domainKernel_t), 0, NULL_HINT, NO_ALLOC);
//     kernelDbPtr->stopTimer = stopTimer;
//     ocrDbRelease(kernelDbGuid);
//     ocrEventSatisfy(kernelEdtDoneEvt, kernelDbGuid);

//     return NULL_GUID;
// }






// #ifdef NULL_EDT_EX

// typedef struct {
//     ocrGuid_t self;
//     ocrGuid_t nullEdtTplGuid;
//     ocrGuid_t events[EVENT_COUNT];
//     timestamp_t startTimer;
// } domainSetup_t;

// typedef struct {
//     timestamp_t stopTimer;
//     long elapsed;
// } domainKernel_t;

// ocrGuid_t nullEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);
// ocrGuid_t combineEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);
// void combine(ocrGuid_t * edtGuid, ocrGuid_t edtTemplGuid, ocrGuid_t affinity,
//              ocrGuid_t firstEvt, ocrGuid_t secondEvt, ocrGuid_t resultEvt);

// void domainSetup(domainSetup_t * dsetup) {
//     ocrGuid_t nullEdtTplGuid;
//     ocrEdtTemplateCreate(&nullEdtTplGuid, nullEdt, 1, EVENT_COUNT);
//     dsetup->nullEdtTplGuid = nullEdtTplGuid;
//     u32 i = 0;
//     while (i < EVENT_COUNT) {
//         ocrGuid_t evtGuid;
//         ocrEventCreate(&evtGuid, OCR_EVENT_STICKY_T, false);
//         ocrEventSatisfy(evtGuid, NULL_GUID);
//         dsetup->events[i] = evtGuid;
//         i++;
//     }
// }

// void domainKernel(ocrGuid_t kernelEdtDoneEvt, domainSetup_t * setupDbPtr, timestamp_t * timer) {
//     u64 paramv[1];
//     paramv[0] = (u64) kernelEdtDoneEvt;
//     ocrGuid_t * edtDeps = setupDbPtr->events;
//     ocrGuid_t nullEdtTplGuid = setupDbPtr->nullEdtTplGuid;

//     // For now, recreate this everytime. Probably the right solution is to
//     // have some sort of domain's global.
//     u64 affCount = 0;
//     ocrAffinityCount(AFFINITY_PD, &affCount);
//     ocrGuid_t affinities[affCount];
//     ocrAffinityGet(AFFINITY_PD, &affCount, affinities);

//     // Time stamp start, the executed EDT stamps stop.
//     get_time(&setupDbPtr->startTimer);

//     // This EDT satisfies kernelEdtDoneEvt
//     ocrEdtCreate(NULL, nullEdtTplGuid, 1, paramv, EVENT_COUNT, edtDeps, EDT_PROP_NONE, affinities[affCount-1], NULL);
// }

// void domainCombine(domainSetup_t * setupDbPtr, domainKernel_t * kernelPtr, long * elapsed) {
//     *elapsed = elapsed_usec(&setupDbPtr->startTimer, &kernelPtr->stopTimer);
//     // Local clean-up
//     ocrEdtTemplateDestroy(setupDbPtr->nullEdtTplGuid);
//     u32 i = 0;
//     while (i < EVENT_COUNT) {
//         ocrEventDestroy(setupDbPtr->events[i]);
//         i++;
//     }
// }

// // - Completion event to be satisfied when execution is done (paramv[0])
// ocrGuid_t nullEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
//     // PRINTF("nullEdt\n");
//     // Stop the timer right away since we measure the spawn to execution time.
//     timestamp_t stopTimer;
//     get_time(&stopTimer);

//     ocrGuid_t kernelEdtDoneEvt = (ocrGuid_t) paramv[0];

//     // Done with kernel
//     ocrGuid_t kernelDbGuid;
//     domainKernel_t * kernelDbPtr;
//     //TODO create that on current affinity ?
//     ocrDbCreate(&kernelDbGuid, (void**) &kernelDbPtr, sizeof(domainKernel_t), 0, NULL_HINT, NO_ALLOC);
//     kernelDbPtr->stopTimer = stopTimer;
//     ocrDbRelease(kernelDbGuid);
//     ocrEventSatisfy(kernelEdtDoneEvt, kernelDbGuid);

//     return NULL_GUID;
// }

// #endif

#define FRWK_REMOTE_SETUP 0

#include "framework.ctpl"
