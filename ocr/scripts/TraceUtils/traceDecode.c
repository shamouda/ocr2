#include <stdio.h>
#include <stdlib.h>

#include "utils/tracer/tracer.h"
#include "utils/tracer/trace-events.h"

#define IDX_OFFSET OCR_TRACE_TYPE_EDT

void translateObject(ocrTraceObj_t *trace);

int main(int argc, char *argv[]){

    if(argc != 2){
        printf("\n-------- Incorrect Input ---------\n");
        printf("Usage: %s <filename>\n\n", argv[0]);
        return 1;
    }

    char *fname = argv[1];
    ocrTraceObj_t trace;
    FILE *f = fopen(fname, "r");
    while(fread(&trace, sizeof(ocrTraceObj_t), 1, f)){
        translateObject(&trace);
    }
    fclose(f);
    return 1;
}

void genericPrint(bool evtType, ocrTraceType_t ttype, ocrTraceAction_t action, u64 location, u64 timestamp, ocrGuid_t parent){

    if(parent != NULL_GUID){
        printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: %s | ACTION: %s | PARENT: 0x%lx\n",
                evt_type[evtType], location, timestamp, obj_type[ttype-IDX_OFFSET], action_type[action], parent);
    }else{

        printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: %s | ACTION: %s\n",
                evt_type[evtType], location, timestamp, obj_type[ttype-IDX_OFFSET], action_type[action]);
    }
}

void translateObject(ocrTraceObj_t *trace){


    ocrTraceType_t  ttype = trace->typeSwitch;
    ocrTraceAction_t action =  trace->actionSwitch;
    u64 timestamp = trace->time;
    u64 location = trace->location;
    bool evtType = trace->eventType;

    switch(trace->typeSwitch){

    case OCR_TRACE_TYPE_EDT:

        switch(trace->actionSwitch){

            case OCR_ACTION_CREATE:
            {
                ocrGuid_t parent = TRACE_FIELD(TASK, taskCreate, trace, parentID);
                genericPrint(evtType, ttype, action, location, timestamp, parent);
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, timestamp, NULL_GUID);
                break;
            case OCR_ACTION_RUNNABLE:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, timestamp, NULL_GUID);
                break;
            case OCR_ACTION_SATISFY:
            {
                ocrGuid_t src = TRACE_FIELD(TASK, taskDepSatisfy, trace, depID);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: DEP_SATISFY | DEP_GUID: 0x%lx\n",
                        evt_type[evtType], location, timestamp, (u64)src);
                break;
            }
            case OCR_ACTION_ADD_DEP:
            {
                ocrGuid_t dest = TRACE_FIELD(TASK, taskDepReady, trace, depID);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: ADD_DEP | DEP_GUID: 0x%lx\n",
                        evt_type[evtType], location, timestamp, dest);
                break;
            }
            case OCR_ACTION_EXECUTE:
            {
                ocrEdt_t funcPtr = TRACE_FIELD(TASK, taskExeBegin, trace, funcPtr);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: EXECUTE | FUNC_PTR: 0x%lx\n",
                        evt_type[evtType], location, timestamp, (u64)funcPtr);
                break;
            }
            case OCR_ACTION_FINISH:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_DATA_ACQUIRE:
            {
                ocrGuid_t edtGuid = TRACE_FIELD(TASK, taskDataAcquire, trace, taskGuid);
                ocrGuid_t dbGuid = TRACE_FIELD(TASK, taskDataAcquire, trace, dbGuid);
                u64 size = TRACE_FIELD(TASK, taskDataAcquire, trace, size);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: DB_ACQUIRE | EDT_GUID: 0x%lx | DB_GUID: 0x%lx | DB_SIZE: %lu\n",
                        evt_type[evtType], location, timestamp, edtGuid, dbGuid, size);

                break;
            }

            case OCR_ACTION_DATA_RELEASE:
            {
                ocrGuid_t edtGuid = TRACE_FIELD(TASK, taskDataRelease, trace, taskGuid);
                ocrGuid_t dbGuid = TRACE_FIELD(TASK, taskDataRelease, trace, dbGuid);
                u64 size = TRACE_FIELD(TASK, taskDataRelease, trace, size);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EDT | ACTION: DB_RELEASE | EDT_GUID: 0x%lx | DB_GUID: 0x%lx | DB_SIZE: %lu\n",
                        evt_type[evtType], location, timestamp, edtGuid, dbGuid, size);

                break;
            }

        }
        break;

    case OCR_TRACE_TYPE_EVENT:

        switch(trace->actionSwitch){

            case OCR_ACTION_CREATE:
            {
                ocrGuid_t parent = TRACE_FIELD(EVENT, eventCreate, trace, parentID);
                genericPrint(evtType, ttype, action, location, timestamp, parent);
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_SATISFY:
            {
                ocrGuid_t src = TRACE_FIELD(EVENT, eventDepSatisfy, trace, depID);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EVENT | ACTION: DEP_SATISFY | DEP_GUID: 0x%lx\n",
                       evt_type[evtType], location, timestamp, (u64)src);
                break;
            }

            case OCR_ACTION_ADD_DEP:
            {
                ocrGuid_t dest = TRACE_FIELD(EVENT, eventDepAdd, trace, depID);
                ocrGuid_t parent = TRACE_FIELD(EVENT, eventDepAdd, trace, parentID);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: EVENT | ACTION: ADD_DEP | DEP_GUID: 0x%lx | PARENT: 0x%lx\n",
                        evt_type[evtType], location, timestamp, dest, parent);
            }
                break;

            default:
                break;
        }
        break;

    case OCR_TRACE_TYPE_DATABLOCK:

        switch(trace->actionSwitch){

            case OCR_ACTION_CREATE:
            {
                ocrGuid_t parent = TRACE_FIELD(DATA, dataCreate, trace, parentID);
                u64 size = TRACE_FIELD(DATA, dataCreate, trace, size);
                printf("[TRACE] U/R: %s | LOCATION: 0x%lx | TIMESTAMP: %lu | TYPE: DATABLOCK | ACTION: CREATE | SIZE: %lu | PARENT:     0x%lx\n",
                        evt_type[evtType], location, timestamp, size, parent);
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, timestamp, NULL_GUID);
                break;

            default:
                break;
        }
        break;
    }
}

