#include "ocr-config.h"
#include <stdarg.h>

#include "ocr-sal.h"
#include "utils/ocr-utils.h"
#include "utils/tracer/tracer.h"
#include "ocr-types.h"

//Offset needed for collision avoiding typedef'd values for trace object types
#define IDX_OFFSET OCR_TRACE_TYPE_EDT

//Strings to identify user/runtime created objects
const char *evt_type[] = {
    "RUNTIME",
    "USER",
};

//Strings for traced OCR objects
const char *obj_type[] = {
    "EDT",
    "EVENT",
    "DATABLOCK"
};

//Strings for traced OCR events
const char *action_type[] = {
    "CREATE",
    "DESTROY",
    "RUNNABLE",
    "ADD_DEP",
    "SATISFY",
    "EXECUTE",
    "FINISH",
};

void doTrace(u64 location, u64 wrkr, ocrGuid_t parent, char *str, ...){
    //TODO implement timing in sal and replace: for platform independence
    //(patch currently pending for this in gerrit)
    u64 timestamp = salGetTime();

    va_list ap;
    va_start(ap, str);

    //Find number of vairable for non-trace format string.
    u32 numV = numVarsInString(str);
    u32 i;

    //Skip over non trace string vairables.
    //**NOTE: Since this is infrastructure is internally managed, it is assumed that as tracing extends
    //to further types the program will maintain the use of 64-bit values as trace arguments. Otherwise
    //grabbing va_args as a void * can cause arguments to be skipped. See Redmine issue #823
    for(i = 0; i < numV; i++){
        va_arg(ap, void *);
    }

    //TODO: replace u32 with bool when system worker is integrated.  As of now the va_args get parsed
    //even if no trace information is provided.  Once system worker is in place, this function will
    //no-op based on a check for whether system worker is configured. See Redmine issue #824
    bool isUserType = va_arg(ap, u32);

    ocrTraceType_t objType = va_arg(ap, ocrTraceType_t);
    ocrTraceAction_t actionType = va_arg(ap, ocrTraceAction_t);

    populateTraceObject(isUserType, objType, actionType, location, timestamp, parent, ap);

    va_end(ap);

}

//TODO: Return a Trace object for deque when implemented.
void populateTraceObject(bool isUserType, ocrTraceType_t objType, ocrTraceAction_t actionType, u64 location,
                        u64 timestamp, ocrGuid_t parent, va_list ap){
    ocrGuid_t src = NULL_GUID;
    ocrGuid_t dest = NULL_GUID;
    ocrEdt_t func = NULL_GUID;
    u64 size = 0;

    ocrTraceObj_t tr;
    tr.location = location;
    tr.time = timestamp;
    tr.isUserType = isUserType;

    //Look at each trace type case by case and populate traceObject fields.
    //Unused trace types currently ommited from switch until needed/supported by runtime.
    //Unused struct/union fields currently initialized to NULL or 0.
    switch(objType){

    case OCR_TRACE_TYPE_EDT:
        switch(actionType){

            case OCR_ACTION_CREATE:
                tr.type.task.action.taskCreate.parentID = parent;
                genericPrint(isUserType, objType, actionType, location, timestamp, parent);
                break;

            case OCR_ACTION_DESTROY:
                tr.type.task.action.taskDestroy.placeHolder = NULL;
                genericPrint(isUserType, objType, actionType, location, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_RUNNABLE:
                tr.type.task.action.taskReadyToRun.whyReady = 0;
                genericPrint(isUserType, objType, actionType, location, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_SATISFY:
                // 1 va_arg needed.
                src = va_arg(ap, ocrGuid_t);
                tr.type.task.action.taskDepSatisfy.depID = src;
                PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: DEP_SATISFY | DEP_GUID: 0x%llx\n",
                        evt_type[isUserType], location, timestamp, src);
                break;

            case OCR_ACTION_ADD_DEP:
                dest = va_arg(ap, ocrGuid_t);
                tr.type.task.action.taskDepReady.depID = dest;
                tr.type.task.action.taskDepReady.parentPermissions = 0;
                PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EVENT | ACTION: ADD_DEP | DEP_GUID: 0x%lx\n",
                        evt_type[isUserType], location, timestamp, dest);
                break;

            case OCR_ACTION_EXECUTE:
                // 1 va_arg needed
                func = va_arg(ap, ocrEdt_t);
                tr.type.task.action.taskExeBegin.funcPtr = func;
                tr.type.task.action.taskExeBegin.whyDelay = 0;
                PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: EXECUTE | FUNC_PTR: 0x%llx\n",
                        evt_type[isUserType], location, timestamp, func);
                break;

            case OCR_ACTION_FINISH:
                tr.type.task.action.taskExeEnd.placeHolder = NULL;
                genericPrint(isUserType, objType, actionType, location, timestamp, NULL_GUID);
                break;
        }
        break;

    case OCR_TRACE_TYPE_EVENT:
        switch(actionType){

            case OCR_ACTION_CREATE:
                tr.type.event.action.eventCreate.parentID = parent;
                genericPrint(isUserType, objType, actionType, location, timestamp, parent);
                break;

            case OCR_ACTION_DESTROY:
                tr.type.event.action.eventDestroy.placeHolder = NULL;
                genericPrint(isUserType, objType, actionType, location, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_ADD_DEP:
                // 1 va_arg needed
                dest = va_arg(ap, ocrGuid_t);
                tr.type.event.action.eventDepAdd.depID = dest;
                tr.type.event.action.eventDepAdd.parentID = parent;
                PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EVENT | ACTION: ADD_DEP | DEP_GUID: 0x%lx\n",
                        evt_type[isUserType], location, timestamp, dest);
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
                tr.type.data.action.dataCreate.parentID = parent;
                tr.type.data.action.dataCreate.size = size;
                PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: DATABLOCK | ACTION: CREATE | SIZE: %lu | PARENT: 0x%lx\n",
                        evt_type[isUserType], location, timestamp, size, parent);
                break;

            case OCR_ACTION_DESTROY:
                tr.type.data.action.dataDestroy.placeHolder = NULL;
                genericPrint(isUserType, objType, actionType, location, timestamp, NULL_GUID);
                break;

            default:
                break;
        }
        break;
    }
}

//Returns number of va_args expected by format string.
//TODO: Possible bug if DPRINTF escapes to actually print a '%'.
//      Currently none do.
u32 numVarsInString(char *fmt){
    u32 i;
    for(i = 0; fmt[i]; fmt[i]=='%' ? i++ : *fmt++);
    return i;
}

//Print TRACE object that only uses args provided by the DPRINTF macro (I.e. no trace ar_args needed)
static void genericPrint(bool isUserType, ocrTraceType_t trace, ocrTraceAction_t action, u64 location, u64 timestamp, ocrGuid_t parent){
    if(parent != NULL_GUID){
        PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: %s | ACTION: %s | PARENT: 0x%lx\n",
                evt_type[isUserType], location, timestamp, obj_type[trace-IDX_OFFSET], action_type[action], parent);
                //location, timestamp, traceTypeToString(trace), actionTypeToString(action), parent);
    }else{

        PRINTF("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: %s | ACTION: %s\n",
                evt_type[isUserType], location, timestamp, obj_type[trace-IDX_OFFSET], action_type[action]);
                //location, timestamp, traceTypeToString(trace), actionTypeToString(action));
    }

}
