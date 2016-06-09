#include "perfs.h"
#include "ocr.h"
#include "extensions/ocr-affinity.h"

// DESC: A local EDT depends on a remote DB
// TIME: Time between creation of the EDT its execution + release deps
// FREQ: Do NB_ITERS calls
//
// VARIABLES
// - NB_ITERS
// - DB_SZ
// - DB_MODE

//
// !! Work In Progress !!
//
// Trying to define a framework to do the setup and execution of benchmarks.
//

#ifndef NB_ITERS
#define NB_ITERS 1000
#endif

#ifndef DB_SZ
#define DB_SZ 1024
#endif

#ifndef DB_MODE
#define DB_MODE DB_MODE_RW
#endif

#define TIME_RELEASE2 0

#ifndef TIME_RELEASE2
#define TIME_RELEASE2 1
#endif

#define THROUGHPUT_METRIC NB_ITERS

//
// User Part
//

typedef struct {
    //TODO: there should be a sub struct that we extend so that the framework can do the setup
    ocrGuid_t self;
    // end common
    //
    ocrGuid_t computeTpl;
    ocrGuid_t userSetupDoneEvt;
    ocrGuid_t remoteDb; // the remote DB to acquire
} domainSetup_t;

typedef struct {
    ocrGuid_t self;
    timestamp_t startTimer;
    timestamp_t stopTimer;
} domainKernel_t;

ocrGuid_t remoteSetupUserEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t dsetupGuid = depv[0].guid;
    domainSetup_t * dsetup = (domainSetup_t *) depv[0].ptr;

    // Create 'remote' (local to here) db
    ocrGuid_t dbGuid;
    char * ptr;
    ocrDbCreate(&dbGuid, (void **)&ptr, sizeof(char)*(DB_SZ), 0, NULL_HINT, NO_ALLOC);
    ocrDbRelease(dbGuid);
    dsetup->remoteDb = dbGuid;

    ocrGuid_t userSetupDoneEvt = dsetup->userSetupDoneEvt;
    ocrDbRelease(dsetupGuid);

    // Global setup is done
    ocrEventSatisfy(userSetupDoneEvt, NULL_GUID);

    return NULL_GUID;
}

ocrGuid_t computeEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrGuid_t kernelDb = {.guid = paramv[0]};
    return kernelDb;
}

// Create an event at the current affinity and writes
// the GUID into the domainSetup data-structure.
void domainSetup(ocrGuid_t userSetupDoneEvt, domainSetup_t * dsetup) {
    // This is for the domain kernel to callback and stop the timer
    // ocrGuid_t stopTimerEvt;
    // ocrEventCreate(&stopTimerEvt, OCR_EVENT_ONCE_T, true);

    // Create an EDT at a remote affinity to:
    // - Create a remote latch event and initialize it
    // - Hook that event to the local event declared above
    // - write back the guid of the remote latch event into the setup DB

    // - userSetupDoneEvt: to be satisfied when setup is done
    // - stopTimerEvt: to be satisfied when domain kernel is done
    ocrEdtTemplateCreate(&dsetup->computeTpl, computeEdt, 1, 1);
    dsetup->userSetupDoneEvt = userSetupDoneEvt;
    // dsetup->stopTimerEvt = stopTimerEvt;

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

    // Create local EDT that depends on remote DB
    get_time(&kernelDbPtr->startTimer);
    ocrDbRelease(kernelDbGuid);

    ocrHint_t edtHint;
    ocrHintInit(&edtHint, OCR_HINT_EDT_T);
    ocrSetHintValue(&edtHint, OCR_HINT_EDT_AFFINITY, ocrAffinityToHintValue(curAffGuid));
    ocrGuid_t edtGuid;
    ocrGuid_t outEvt;
    u64 paramv = (u64) kernelDbGuid.guid;
    ocrEdtCreate(&edtGuid, setupDbPtr->computeTpl,
                 1, &paramv, 1, NULL, EDT_PROP_NONE, &edtHint, &outEvt);
    ocrAddDependence(outEvt, userKernelDoneEvt, 0, DB_MODE_NULL);
    ocrAddDependence(setupDbPtr->remoteDb, edtGuid, 0, DB_MODE);
}

void domainKernelCombine(domainSetup_t * setupDbPtr, domainKernel_t * kernelPtr, long * elapsed) {
    get_time(&kernelPtr->stopTimer);
    *elapsed = elapsed_usec(&kernelPtr->startTimer, &kernelPtr->stopTimer);
    ocrEdtTemplateDestroy(setupDbPtr->computeTpl);
    ocrDbDestroy(setupDbPtr->remoteDb);
}

#define FRWK_REMOTE_SETUP 0

#include "framework.ctpl"
