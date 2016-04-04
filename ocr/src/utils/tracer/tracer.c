#include "ocr-config.h"
#ifdef ENABLE_WORKER_SYSTEM

#include <stdarg.h>

#include "ocr-sal.h"
#include "utils/ocr-utils.h"
#include "ocr-types.h"
#include "utils/tracer/tracer.h"
#include "utils/deque.h"
#include "worker/hc/hc-worker.h"


static bool isDequeFull(deque_t *deq){
    if(deq == NULL) return false;
    s32 head = deq->head;
    s32 tail = deq->tail;
    if(tail == (INIT_DEQUE_CAPACITY + head)){
        return true;
    }else{
        return false;
    }
}

static bool isSystem(ocrPolicyDomain_t *pd){
    u32 idx = (pd->workerCount)-1;
    ocrWorker_t *wrkr = pd->workers[idx];
    if(wrkr->type == SYSTEM_WORKERTYPE){
        return true;
    }else{
        return false;
    }
}

static bool isSupportedTraceType(bool evtType, ocrTraceType_t ttype, ocrTraceAction_t atype){
    //Hacky sanity check to ensure va_arg got valid trace info if provided.
    //return true if supported (list will expand as more trace types become needed/supported)
    return ((ttype >= OCR_TRACE_TYPE_EDT && ttype <= OCR_TRACE_TYPE_DATABLOCK) &&
            (atype >= OCR_ACTION_CREATE  && atype < OCR_ACTION_MAX) &&
            (evtType == true || evtType == false));
}

//Create a trace object subject to trace type, and push to HC worker's deque, to be processed by system worker.
static void populateTraceObject(u64 location, bool evtType, ocrTraceType_t objType, ocrTraceAction_t actionType,
                                u64 workerId, u64 timestamp, ocrGuid_t parent, va_list ap){


    ocrGuid_t src = NULL_GUID;
    ocrGuid_t dest = NULL_GUID;
    ocrEdt_t func = NULL;
    u64 size = 0;

    ocrPolicyDomain_t *pd = NULL;
    ocrWorker_t *worker;
    getCurrentEnv(&pd, &worker, NULL, NULL);

    ocrTraceObj_t * tr = pd->fcts.pdMalloc(pd, sizeof(ocrTraceObj_t));

    //Populate fields common to all trace objects.
    tr->typeSwitch = objType;
    tr->actionSwitch = actionType;
    tr->workerId = workerId;
    tr->location = location;
    tr->time = timestamp;
    tr->eventType = evtType;

    //Look at each trace type case by case and populate traceObject fields.
    //Unused trace types currently ommited from switch until needed/supported by runtime.
    //Unused struct/union fields currently initialized to NULL or 0 as placeholders.
    switch(objType){

    case OCR_TRACE_TYPE_EDT:

        switch(actionType){

            case OCR_ACTION_CREATE:
                TRACE_FIELD(TASK, taskCreate, tr, parentID) = parent;
                break;

            case OCR_ACTION_DESTROY:
                TRACE_FIELD(TASK, taskDestroy, tr, placeHolder) = NULL;
                break;

            case OCR_ACTION_RUNNABLE:
                TRACE_FIELD(TASK, taskReadyToRun, tr, whyReady) = 0;
                break;

            case OCR_ACTION_SATISFY:
                // 1 va_arg needed.
                src = va_arg(ap, ocrGuid_t);
                TRACE_FIELD(TASK, taskDepSatisfy, tr, depID) = src;
                break;

            case OCR_ACTION_ADD_DEP:
                //1 va_arg needed
                dest = va_arg(ap, ocrGuid_t);
                TRACE_FIELD(TASK, taskDepReady, tr, depID) = dest;
                TRACE_FIELD(TASK, taskDepReady, tr, parentPermissions) = 0;
                break;

            case OCR_ACTION_EXECUTE:
                // 1 va_arg needed
                func = va_arg(ap, ocrEdt_t);
                TRACE_FIELD(TASK, taskExeBegin, tr, funcPtr) = func;
                TRACE_FIELD(TASK, taskExeBegin, tr, whyDelay) = 0;
                break;

            case OCR_ACTION_FINISH:
                TRACE_FIELD(TASK, taskExeEnd, tr, placeHolder) = NULL;
                break;

            case OCR_ACTION_DATA_ACQUIRE:
            {
                ocrGuid_t edtGuid = va_arg(ap, ocrGuid_t);
                ocrGuid_t dbGuid = va_arg(ap, ocrGuid_t);
                u64 dbSize = va_arg(ap, u64);
                TRACE_FIELD(TASK, taskDataAcquire, tr, taskGuid) = edtGuid;
                TRACE_FIELD(TASK, taskDataAcquire, tr, dbGuid) = dbGuid;
                TRACE_FIELD(TASK, taskDataAcquire, tr, size) = dbSize;
                break;
            }

            case OCR_ACTION_DATA_RELEASE:
            {
                ocrGuid_t edtGuid = va_arg(ap, ocrGuid_t);
                ocrGuid_t dbGuid = va_arg(ap, ocrGuid_t);
                u64 dbSize = va_arg(ap, u64);
                TRACE_FIELD(TASK, taskDataRelease, tr, taskGuid) = edtGuid;
                TRACE_FIELD(TASK, taskDataRelease, tr, dbGuid) = dbGuid;
                TRACE_FIELD(TASK, taskDataRelease, tr, size) = dbSize;
                break;
            }

            default:
                break;

        }
        break;

    case OCR_TRACE_TYPE_EVENT:

        switch(actionType){

            case OCR_ACTION_CREATE:
                TRACE_FIELD(EVENT, eventCreate, tr, parentID) = parent;
                break;

            case OCR_ACTION_DESTROY:
                TRACE_FIELD(EVENT, eventDestroy, tr, placeHolder) = NULL;
                break;

            case OCR_ACTION_ADD_DEP:
                // 1 va_arg needed
                dest = va_arg(ap, ocrGuid_t);
                TRACE_FIELD(EVENT, eventDepAdd, tr, depID) = dest;
                TRACE_FIELD(EVENT, eventDepAdd, tr, parentID) = parent;
                break;

            case OCR_ACTION_SATISFY:
                src = va_arg(ap, ocrGuid_t);
                TRACE_FIELD(EVENT, eventDepSatisfy, tr, depID) = src;
                break;

            default:
                break;
        }

        break;

    case OCR_TRACE_TYPE_DATABLOCK:

        switch(actionType){

            case OCR_ACTION_CREATE:
                // 1 va_args needed
                size = va_arg(ap, u64);
                TRACE_FIELD(DATA, dataCreate, tr, parentID) = parent;
                TRACE_FIELD(DATA, dataCreate, tr, size) = size;
                break;

            case OCR_ACTION_DESTROY:
                TRACE_FIELD(DATA, dataDestroy, tr, placeHolder) = NULL;
                break;

            default:
                break;
        }
        break;
    }

    while(isDequeFull(((ocrWorkerHc_t*)worker)->sysDeque)){
        hal_pause();
    }

    //Trace object populated. Push to my deque
    ((ocrWorkerHc_t *)worker)->sysDeque->pushAtTail(((ocrWorkerHc_t *)worker)->sysDeque, tr, 0);

}

void doTrace(u64 location, u64 wrkr, ocrGuid_t parent, ...){
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);

    //First check if a system worker is configured.  If not, return and do nothing;
    if((pd == NULL) || !(isSystem(pd))) return;

    u64 timestamp = salGetTime();

    va_list ap;
    va_start(ap, parent);

    //Retrieve event type and action of trace. By convention in the order below.
    bool evtType = va_arg(ap, u32);
    ocrTraceType_t objType = va_arg(ap, ocrTraceType_t);
    ocrTraceAction_t actionType = va_arg(ap, ocrTraceAction_t);

    //If no valid additional tracing info found return to normal DPRINTF
    if(!(isSupportedTraceType(evtType, objType, actionType))){
        va_end(ap);
        return;
    }
    populateTraceObject(location, evtType, objType, actionType, wrkr, timestamp, parent, ap);
    va_end(ap);

}
#endif /* ENABLE_WORKER_SYSTEM */
