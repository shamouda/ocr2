/*
* This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_EXTENSION_LEGACY

#include "debug.h"
#include "extensions/ocr-legacy.h"
#include "ocr-hal.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "ocr-worker.h"

#pragma message "LEGACY extension is experimental and may not be supported on all platforms"

#define DEBUG_TYPE API

extern void freeUpRuntime(void);
extern void bringUpRuntime(const char*);

void ocrLegacyInit(ocrGuid_t *legacyContext, ocrConfig_t * ocrConfig) {
    // Bug #492: legacyContext is ignored
    ASSERT(ocrConfig);
    if(ocrConfig->iniFile == NULL)
        PRINTF("ERROR: Set OCR_CONFIG to point to OCR configuration file\n");
    ASSERT(ocrConfig->iniFile);
    bringUpRuntime(ocrConfig->iniFile);
}

u8 ocrLegacyFinalize(ocrGuid_t legacyContext, bool runUntilShutdown) {
    // Bug #492: legacyContext is ignored
    ocrPolicyDomain_t *pd = NULL;
    ocrWorker_t *worker = NULL;
    u8 returnCode;
    if(runUntilShutdown) {
        getCurrentEnv(&pd, &worker, NULL, NULL);
        // We start the current worker. After it starts, it will loop
        // until ocrShutdown is called which will cause the entire PD
        // to stop (including this worker). The currently executing
        // worker then fallthrough from start to finish.
        worker->fcts.start(worker, pd);

        // When the worker exits 'start' the PD runlevel has been
        // decremented to RL_STOP
        // Transition from stop to shutdown
        pd->fcts.setRunlevel(pd, RL_SHUTDOWN);
        returnCode = pd->shutdownCode;
        pd->fcts.setRunlevel(pd, RL_DEALLOCATE);
    } else {
        getCurrentEnv(&pd, NULL, NULL, NULL);
        returnCode = pd->shutdownCode;
    }
    freeUpRuntime();
// #ifdef OCR_ENABLE_STATISTICS
//     ocrStatsProcessDestruct(&GfakeProcess);
//     GocrFilterAggregator->destruct(GocrFilterAggregator);
// #endif
    return returnCode;
}

u8 ocrLegacySpawnOCR(ocrGuid_t* handle, ocrGuid_t finishEdtTemplate, u64 paramc, u64* paramv,
                     u64 depc, ocrGuid_t* depv, ocrGuid_t legacyContext) {

    // Bug #492: legacyContext is ignored
    // Bug #494: a thread is lost since this function spins
    ocrGuid_t edtGuid;
    ocrGuid_t stickyGuid;
    ocrGuid_t outputEventGuid;
    ocrGuid_t depv0;
    u32 i;
    ocrEventCreate(&stickyGuid, OCR_EVENT_STICKY_T, true);
    // OCR codes do not have access to any data from heap/stack so things need to
    // be marshalled in. We also rely on depc >= 1 to "hold-off" the start of the EDT
    // and properly setup the dependence chain
    ASSERT(depc >= 1);

    for(i=0; i < depc; ++i) {
        // Verify that all dependences are known
        ASSERT(depv[i] != UNINITIALIZED_GUID);
    }

    // Hold off the start by saying that one dependence is not satisfied
    depv0 = depv[0];
    depv[0] = UNINITIALIZED_GUID;
    ocrEdtCreate(&edtGuid, finishEdtTemplate, paramc, paramv,
                 depc, depv, EDT_PROP_FINISH, NULL_GUID, &outputEventGuid);
    ocrAddDependence(outputEventGuid, stickyGuid, 0, DB_DEFAULT_MODE);
    // Actually start the OCR EDT
    ocrAddDependence(depv0, edtGuid, 0, DB_DEFAULT_MODE);
    *handle = stickyGuid;
    return 0;
}

ocrGuid_t ocrWait(ocrGuid_t outputEvent) {
    //DPRINTF(DEBUG_LVL_WARN, "ocrWait is deprecated -- use ocrLegacyBlockProgress instead\n");
    ocrGuid_t outputGuid;
    if(ocrLegacyBlockProgress(outputEvent, &outputGuid, NULL, NULL) == 0) {
        return outputGuid;
    }
    return ERROR_GUID;
}

u8 ocrLegacyBlockProgress(ocrGuid_t handle, ocrGuid_t* guid, void** result, u64* size) {
    ocrPolicyDomain_t *pd = NULL;
    ocrEvent_t *eventToYieldFor = NULL;
    ocrFatGuid_t dbResult = {.guid = ERROR_GUID, .metaDataPtr = NULL};
    ocrFatGuid_t currentEdt;
    ocrTask_t *curTask = NULL;

    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    currentEdt.guid = (curTask == NULL) ? NULL_GUID : curTask->guid;
    currentEdt.metaDataPtr = curTask;

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
    msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = handle;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(properties) = KIND_GUIDPROP | RMETA_GUIDPROP;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
    eventToYieldFor = (ocrEvent_t *)PD_MSG_FIELD_IO(guid.metaDataPtr);
#undef PD_TYPE

    // We can't wait on once/latch events since they could get freed asynchronously
    ASSERT(eventToYieldFor->kind == OCR_EVENT_STICKY_T ||
           eventToYieldFor->kind == OCR_EVENT_IDEM_T);
    do {
        hal_pause();
        dbResult.guid = ERROR_GUID;
        dbResult = pd->eventFactories[0]->fcts[eventToYieldFor->kind].get(eventToYieldFor);
    } while(dbResult.guid == ERROR_GUID);

    if(dbResult.guid != NULL_GUID) {
        if(dbResult.metaDataPtr == NULL) {
            // We now need to acquire the DB
            getCurrentEnv(NULL, NULL, NULL, &msg);
#define PD_TYPE PD_MSG_DB_ACQUIRE
            msg.type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
            PD_MSG_FIELD_IO(guid) = dbResult;
            PD_MSG_FIELD_IO(edt) = currentEdt;
            PD_MSG_FIELD_IO(properties) = DB_MODE_ITW;
            RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));

            if(result != NULL)
                *result = PD_MSG_FIELD_O(ptr);
            dbResult = PD_MSG_FIELD_IO(guid);
#undef PD_TYPE
#undef PD_MSG
        } else {
            if(result != NULL)
                *result = ((ocrDataBlock_t*)(dbResult.metaDataPtr))->ptr;
        }

        ASSERT(dbResult.metaDataPtr != NULL);
        if(size != NULL)
            *size = ((ocrDataBlock_t*)(dbResult.metaDataPtr))->size;
    }

    if(guid != NULL)
        *guid = dbResult.guid;
    return 0;
}

#endif /* ENABLE_EXTENSION_LEGAGY */
