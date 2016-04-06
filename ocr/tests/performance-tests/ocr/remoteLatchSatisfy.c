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
// #define NB_SATISFY 1048576
#define NB_SATISFY 1024
// #define NB_SATISFY 65536

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


//
// Framework Part
//

ocrGuid_t combineKernelEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);
ocrGuid_t combineSetupEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]);

void combine(ocrGuid_t * edtGuid, ocrGuid_t edtTemplGuid, ocrGuid_t affinity,
             ocrGuid_t firstEvt, ocrGuid_t secondEvt, ocrGuid_t resultEvt);

// Input
// - Completion event to be satisfied when setup is done (paramv[0])
ocrGuid_t setupEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Setup all done event
    ocrGuid_t setupEdtDoneEvt;
    setupEdtDoneEvt.guid = paramv[0];

    ocrGuid_t curAffGuid;
    ocrAffinityGetCurrent(&curAffGuid);

    ocrGuid_t setupDbGuid;
    domainSetup_t * setupDbPtr;
    ocrHint_t dbHint;
    ocrHintInit(&dbHint, OCR_HINT_DB_T);
    ocrSetHintValue(&dbHint, OCR_HINT_DB_AFFINITY, ocrAffinityToHintValue(curAffGuid));
    ocrDbCreate(&setupDbGuid, (void**) &setupDbPtr, sizeof(domainSetup_t), 0, &dbHint, NO_ALLOC);
    setupDbPtr->self = setupDbGuid;

    // This EDT done event
    ocrGuid_t selfDoneEvt;
    ocrEventCreate(&selfDoneEvt, OCR_EVENT_ONCE_T, true);

    // Create a done event for the user code
    ocrGuid_t subSetupDoneEvt;
    ocrEventCreate(&subSetupDoneEvt, OCR_EVENT_ONCE_T, true);
    ocrGuid_t combEdtTplGuid;
    ocrEdtTemplateCreate(&combEdtTplGuid, combineSetupEdt, 1, 2);
    ocrGuid_t combineEdtGuid;
    combine(&combineEdtGuid, combEdtTplGuid, curAffGuid,
                  selfDoneEvt, subSetupDoneEvt, setupEdtDoneEvt);
    ocrEdtTemplateDestroy(combEdtTplGuid);
    domainSetup(subSetupDoneEvt, setupDbPtr);

    ocrDbRelease(setupDbGuid);
    ocrEventSatisfy(selfDoneEvt, setupDbGuid);

    return NULL_GUID;
}

// paramv[0]: event to satisfy when kernel is done
// depv[0]: setupEdt completed (may carry a DB)
ocrGuid_t kernelEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // PRINTF("kernelEdt\n");
    ocrGuid_t kernelEdtDoneEvt;
    kernelEdtDoneEvt.guid = paramv[0];
    ocrGuid_t setupDb = depv[0].guid;
    domainSetup_t * setupDbPtr = depv[0].ptr;

    // The sub kernel done event
    ocrGuid_t subKernelDoneEvt;
    ocrEventCreate(&subKernelDoneEvt, OCR_EVENT_ONCE_T, true);

    // This EDT done event
    ocrGuid_t selfDoneEvt;
    ocrEventCreate(&selfDoneEvt, OCR_EVENT_ONCE_T, true);

    // Combine those in a combine EDT that satisfies kernelEdtDoneEvt
    //TODO same issue of allocating tpl every iteration
    ocrGuid_t curAffGuid;
    ocrAffinityGetCurrent(&curAffGuid);
    ocrGuid_t combEdtTplGuid;
    ocrEdtTemplateCreate(&combEdtTplGuid, combineKernelEdt, 1, 2);
    ocrGuid_t combineEdtGuid;
    combine(&combineEdtGuid, combEdtTplGuid, curAffGuid,
                  selfDoneEvt, subKernelDoneEvt, kernelEdtDoneEvt);
    ocrEdtTemplateDestroy(combEdtTplGuid);

    timestamp_t timer;
    domainKernel(subKernelDoneEvt, setupDbPtr, &timer);

    // Satisfy self event with the timer information
    ocrEventSatisfy(selfDoneEvt, setupDb);
    return NULL_GUID;
}

// ocrGuid_t cleanUpEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
//     PRINTF("cleanUpEdt\n");
//     ocrGuid_t kernelDbGuid = depv[0].guid;
//     ocrGuid_t setupDbGuid = depv[1].guid;

//     ocrDbRelease(kernelDbGuid);
//     ocrDbRelease(setupDbGuid);
//     ocrDbDestroy(kernelDbGuid);
//     ocrDbDestroy(setupDbGuid);
//     return NULL_GUID;
// }

ocrGuid_t postEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // PRINTF("postEdt timeDbGuid=0x%lx\n", depv[0].guid);
    ocrGuid_t postEdtDoneEvt;
    postEdtDoneEvt.guid = paramv[0];
    ocrGuid_t timeDbGuid = depv[0].guid;
    long * timeDbPtr = depv[0].ptr;
    //TODO I think there's an issue here with this
    //edt doing an implicit release at the end
    //concurrently with the iterationEdt destroying it.
    ocrDbRelease(depv[0].guid);
    // domainPost(kernelDbPtr);
    ocrEventSatisfy(postEdtDoneEvt, timeDbGuid);
    return NULL_GUID;
}

#define ITER_IDX 0

#define SETUP_IDX 1
#define KERNEL_IDX 2
#define POST_IDX 3
#define PIPE_START SETUP_IDX
#define PIPE_SZ (POST_IDX-SETUP_IDX+1)
#define END_IDX 4
#define NB_TMPL 5

typedef struct _info_t {
    ocrGuid_t self;
    u32 i;
    u32 max;
    ocrGuid_t edtTemplGuids[NB_TMPL];
    u32 edtAffinities[NB_TMPL];
    long timer;
} info_t;

typedef struct _pipeline_info_t {
    long timer;
} pipeline_info_t;

void cleanFramework(info_t * info);


// For control dependence to sync up framework setup and user setup
ocrGuid_t combineSetupEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t doneEvt;
    doneEvt.guid = paramv[0];
    ocrGuid_t setupDbGuid = depv[0].guid;
    ocrDbRelease(setupDbGuid);
    ocrEventSatisfy(doneEvt, setupDbGuid);
    return NULL_GUID;
}

// Takes two events and satisfy a result event
//depv[0] db for domainSetup_t
//depv[1] db for domainKernel_t
ocrGuid_t combineKernelEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // timestamp_t stopTimer;
    // get_time(&stopTimer);
    ocrGuid_t kernelEdtDoneEvt; // Result event
    kernelEdtDoneEvt.guid = paramv[0];

    // Impl-specific here, we know to expect two DBs
    ocrGuid_t setupDb = depv[0].guid;
    domainSetup_t * setupDbPtr = depv[0].ptr;
    ocrGuid_t kernelDb = depv[1].guid;
    domainKernel_t * kernelDbPtr = depv[1].ptr;
    // kernelDbPtr->stopTimer=stopTimer;
    ocrGuid_t timeDbGuid;
    long * timeDbPtr;
    ocrDbCreate(&timeDbGuid, (void**) &timeDbPtr, sizeof(long), 0, NULL_HINT, NO_ALLOC);

    domainKernelCombine(setupDbPtr, kernelDbPtr, timeDbPtr);
    // PRINTF("combineEdt timeDbGuid=0x%lx\n", timeDbGuid);
    ocrDbRelease(timeDbGuid);
    ocrDbRelease(kernelDb);
    ocrDbRelease(setupDb);
    ocrDbDestroy(kernelDb);
    ocrDbDestroy(setupDb);

    ocrEventSatisfy(kernelEdtDoneEvt, timeDbGuid);

    return NULL_GUID;
}

ocrGuid_t endEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t infoGuid = depv[0].guid;
    info_t * info = (info_t *) depv[0].ptr;
    print_throughput("TEST", (info->max * NB_SATISFY), usec_to_sec(info->timer));
    ocrDbRelease(infoGuid);
    ocrDbDestroy(infoGuid);
    ocrShutdown();
    return NULL_GUID;
}

//TODO: This is pretty much 'combine' but with different depv
void iterate(ocrGuid_t * edtGuid, ocrGuid_t edtTemplGuid,
            ocrGuid_t prevDoneEvt, ocrGuid_t dataGuid, ocrGuid_t nextDoneEvt) {
    u64 paramv[1];
    paramv[0] = (u64) nextDoneEvt.guid;
    ocrGuid_t depv[2];
    depv[0] = dataGuid;
    depv[1] = prevDoneEvt;
    ocrEdtCreate(edtGuid, edtTemplGuid,
                 1, paramv, 2, depv, EDT_PROP_NONE, NULL_HINT, NULL);
}

void combine(ocrGuid_t * edtGuid, ocrGuid_t edtTemplGuid, ocrGuid_t affinity,
             ocrGuid_t firstEvt, ocrGuid_t secondEvt, ocrGuid_t resultEvt) {
    u64 paramv[1];
    paramv[0] = (u64) resultEvt.guid;
    ocrGuid_t depv[2];
    depv[0] = firstEvt;
    depv[1] = secondEvt;
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affinity));
    ocrEdtCreate(edtGuid, edtTemplGuid,
                 1, paramv, 2, depv, EDT_PROP_NONE, &edtHint, NULL);
}

void chain(ocrGuid_t * edtGuid, ocrGuid_t edtTemplGuid, ocrGuid_t affinity,
            ocrGuid_t prevDoneEvt, ocrGuid_t nextDoneEvt) {
    u64 paramv[1];
    paramv[0] = (u64) nextDoneEvt.guid;
    ocrGuid_t depv[1];
    depv[0] = prevDoneEvt;
    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(affinity));
    ocrEdtCreate(edtGuid, edtTemplGuid,
                 1, paramv, 1, depv, EDT_PROP_NONE, &edtHint, NULL);
}

// paramv[0]: continuation after iterations
// depv[0]: info
// depv[1]: done event for the work spawned by the iteration, carries the time DB
ocrGuid_t iterationEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t iterationsDoneEvt;
    iterationsDoneEvt.guid = paramv[0];
    ocrGuid_t infoGuid = depv[0].guid;
    info_t * info = (info_t *) depv[0].ptr;
    ocrGuid_t timeDbGuid = depv[1].guid;
    long * timePrevIt = (long *) depv[1].ptr;

    // PRINTF("iteration %d\n", info->i);
    if (timePrevIt != NULL) {
        info->timer += (*timePrevIt);
        ocrDbRelease(timeDbGuid);
        ocrDbDestroy(timeDbGuid);
    }
    if (info->i < info->max) {
        //TODO do a pipeline EDT
        u64 affCount = 0;
        ocrAffinityCount(AFFINITY_PD, &affCount);
        ocrGuid_t affinities[affCount];
        ocrAffinityGet(AFFINITY_PD, &affCount, affinities);

        ocrGuid_t stageInit;
        ocrEventCreate(&stageInit, OCR_EVENT_ONCE_T, false);
        ocrGuid_t stagePrev = stageInit;
        u32 i = PIPE_START;
        while(i < (PIPE_START+PIPE_SZ)) {
            ocrGuid_t stageEdtDoneEvt;
            ocrEventCreate(&stageEdtDoneEvt, OCR_EVENT_ONCE_T, false);
            ocrGuid_t stageEdtGuid;
            //TODO I wonder if we shouldn't give the whole info to a functor and invoke that
            chain(&stageEdtGuid, info->edtTemplGuids[i], affinities[info->edtAffinities[i]], stagePrev, stageEdtDoneEvt);
            stagePrev = stageEdtDoneEvt;
            i++;
        }
        info->i+=1;
        ocrDbRelease(infoGuid);
        ocrGuid_t nextItEdtGuid;
        // 'iterationsDoneEvt' is passed on and on til the last iteration
        iterate(&nextItEdtGuid, info->edtTemplGuids[ITER_IDX],
                /*prev*/stagePrev, /*data*/infoGuid, /*next*/iterationsDoneEvt);
        // Start the pipeline
        ocrEventSatisfy(stageInit, NULL_GUID);
    } else {
       ocrEventSatisfy(iterationsDoneEvt, infoGuid);
    }
    return NULL_GUID;
}

void setupFramework(info_t * info, ocrGuid_t self, u32 maxIt, u32 affCount) {
    info->self = self;
    info->i = 0;
    info->max = maxIt;
    ocrEdtTemplateCreate(&(info->edtTemplGuids[ITER_IDX]), iterationEdt, 1, 2);
    ocrEdtTemplateCreate(&(info->edtTemplGuids[SETUP_IDX]), setupEdt, 1, 1);
    ocrEdtTemplateCreate(&(info->edtTemplGuids[KERNEL_IDX]), kernelEdt, 1, 1);
    ocrEdtTemplateCreate(&(info->edtTemplGuids[POST_IDX]), postEdt, 1, 1);
    ocrEdtTemplateCreate(&(info->edtTemplGuids[END_IDX]), endEdt, 1, 1);

    //TODO don't like this very much but I don't see how to specify
    //edt affinities otherwise.
    u32 i = 0;
    while (i < NB_TMPL) {
        info->edtAffinities[i] = 0;
        i++;
    }
    info->edtAffinities[SETUP_IDX] = affCount-1;
    ASSERT(affCount >= 1);
    info->timer = 0;
}

void cleanFramework(info_t * info) {
    u32 i = 0;
    while (i < NB_TMPL) {
        ocrEdtTemplateDestroy(info->edtTemplGuids[i++]);
    }
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF("mainEdt\n");
    u32 maxIt = NB_ITERS;

    u64 affCount = 0;
    ocrAffinityCount(AFFINITY_PD, &affCount);
    ocrGuid_t affinities[affCount];
    ocrAffinityGet(AFFINITY_PD, &affCount, affinities);

    ocrGuid_t infoDbGuid;
    info_t * infoDbPtr;
    ocrDbCreate(&infoDbGuid, (void**) &infoDbPtr, sizeof(info_t), 0, NULL_HINT, NO_ALLOC);

    setupFramework(infoDbPtr, infoDbGuid, maxIt, (u32) affCount);
    ocrGuid_t iterTemplGuid = infoDbPtr->edtTemplGuids[ITER_IDX];
    ocrGuid_t endTemplGuid  = infoDbPtr->edtTemplGuids[END_IDX];
    ocrGuid_t affinity = affinities[infoDbPtr->edtAffinities[END_IDX]];
    ocrDbRelease(infoDbGuid);

    ocrGuid_t itDoneEvtGuid;
    ocrEventCreate(&itDoneEvtGuid, OCR_EVENT_STICKY_T, false);

    ocrGuid_t iterateEdtGuid;
    iterate(&iterateEdtGuid, iterTemplGuid,
            /*prev*/NULL_GUID, infoDbGuid, /*next*/itDoneEvtGuid);

    ocrGuid_t endEdtGuid;
    chain(&endEdtGuid, endTemplGuid, affinity, itDoneEvtGuid, NULL_GUID);

    return NULL_GUID;
}
