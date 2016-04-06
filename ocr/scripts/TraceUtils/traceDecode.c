#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ocr.h"
#include "utils/tracer/tracer.h"
#include "utils/tracer/trace-events.h"

#define IDX_OFFSET OCR_TRACE_TYPE_EDT
#define MAX_FCT_PTR_LENGTH 16
#define SYS_CALL_COMMAND_LENGTH 256
#define BIN_PATH_LENGTH 128

void translateObject(ocrTraceObj_t *trace, int lineCount, long *fctPtrs);
int getLineCount(FILE *fname);
void readFctPtrsFromBinary(char *binaryPath, int lineCount, long *fctPtrs);
bool fctPtrExists(long lookup, int lineCount, long *fctPtrs);

int main(int argc, char *argv[]){

    if(argc != 3 && argc != 2){
        printf("\n-------- Incorrect Input ---------\n");
        printf("Usage: %s <filename>  optional : <application binary>\n\n", argv[0]);
        return 1;
    }

    char *fname = argv[1];
    char *binaryPath;
    //If binary path provided, store
    if(argc == 3){
        char *binname = argv[2];
        binaryPath = malloc(strlen(binname)+1);
        strcpy(binaryPath, binname);
    }else{
        binaryPath = "";
    }

    ocrTraceObj_t *trace = malloc(sizeof(ocrTraceObj_t));
    //Attempt to open trace binary
    FILE *f = fopen(fname, "r");
    if(f == NULL){
        printf("Error:  Unable to open provided trace binary\n");
        return 1;
    }


    int lineCount = 0;
    if(binaryPath != ""){
        char *sysCommand = malloc(SYS_CALL_COMMAND_LENGTH);
        char path[BIN_PATH_LENGTH];
        strcpy(sysCommand, "nm ");
        strcat(sysCommand, binaryPath);

        //Call nm on optionally provided application binary.
        FILE *fp = popen(sysCommand, "r");
        free(sysCommand);
        if(fp == NULL){
            printf("Error: Unable to open application binary, reverting to default options\n");
        }else{

            lineCount = getLineCount(fp);
            pclose(fp);
        }

    }

    long *fctPtrs = NULL;
    //This indicates that an application binary was provided, and we will differentatie processRequestEdts
    if(lineCount > 0){
        fctPtrs = (long *)malloc(lineCount*(sizeof(long)));
        readFctPtrsFromBinary(binaryPath, lineCount, fctPtrs);
    }

    //Read each trace record, and decode
    while(fread(trace, sizeof(ocrTraceObj_t), 1, f)){
        translateObject(trace, lineCount, fctPtrs);
    }

    fclose(f);
    return 0;
}

void genericPrint(bool evtType, ocrTraceType_t ttype, ocrTraceAction_t action,
                  u64 location, u64 workerId, u64 timestamp, ocrGuid_t parent){

    if(!(ocrGuidIsNull(parent))){
        printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: %s | ACTION: %s | PARENT: "GUIDF"\n",
                evt_type[evtType], location, workerId, timestamp, obj_type[ttype-IDX_OFFSET], action_type[action], GUIDA(parent));
    }else{

        printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: %s | ACTION: %s\n",
                evt_type[evtType], location, workerId, timestamp, obj_type[ttype-IDX_OFFSET], action_type[action]);
    }
}

void translateObject(ocrTraceObj_t *trace, int lineCount, long *fctPtrs){

    ocrTraceType_t  ttype = trace->typeSwitch;
    ocrTraceAction_t action =  trace->actionSwitch;
    u64 timestamp = trace->time;
    u64 location = trace->location;
    u64 workerId = trace->workerId;
    bool evtType = trace->eventType;

    switch(trace->typeSwitch){

    case OCR_TRACE_TYPE_EDT:

        switch(trace->actionSwitch){

            case OCR_ACTION_CREATE:
            {
                ocrGuid_t parent = TRACE_FIELD(TASK, taskCreate, trace, parentID);
                genericPrint(evtType, ttype, action, location, workerId, timestamp, parent);
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, workerId, timestamp, NULL_GUID);
                break;
            case OCR_ACTION_RUNNABLE:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, workerId, timestamp, NULL_GUID);
                break;
            case OCR_ACTION_SATISFY:
            {
                ocrGuid_t src = TRACE_FIELD(TASK, taskDepSatisfy, trace, depID);

                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: DEP_SATISFY | DEP_GUID: "GUIDF"\n",
                        evt_type[evtType], location, workerId, timestamp, GUIDA(src));
                break;
            }
            case OCR_ACTION_ADD_DEP:
            {
                ocrGuid_t dest = TRACE_FIELD(TASK, taskDepReady, trace, depID);
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: ADD_DEP | DEP_GUID: "GUIDF"\n",
                        evt_type[evtType], location, workerId, timestamp, GUIDA(dest));
                break;
            }
            case OCR_ACTION_EXECUTE:
            {
                ocrEdt_t funcPtr = TRACE_FIELD(TASK, taskExeBegin, trace, funcPtr);

                if(fctPtrs != NULL){

                    if(!(fctPtrExists((long)funcPtr, lineCount, fctPtrs))){
                        printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: EXECUTE | FUNC_PTR: <processRequestEdt>\n",
                                evt_type[0], location, workerId, timestamp);
                        break;
                    }
                }

                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: EXECUTE | FUNC_PTR: 0x%lx\n",
                        evt_type[evtType], location, workerId, timestamp, (u64)funcPtr);

                break;
            }
            case OCR_ACTION_FINISH:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, workerId, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_DATA_ACQUIRE:
            {
                ocrGuid_t edtGuid = TRACE_FIELD(TASK, taskDataAcquire, trace, taskGuid);
                ocrGuid_t dbGuid = TRACE_FIELD(TASK, taskDataAcquire, trace, dbGuid);
                u64 size = TRACE_FIELD(TASK, taskDataAcquire, trace, size);
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: DB_ACQUIRE | EDT_GUID: "GUIDF" | DB_GUID: "GUIDF" | DB_SIZE: %lu\n",
                        evt_type[evtType], location, workerId, timestamp, GUIDA(edtGuid), GUIDA(dbGuid), size);

                break;
            }

            case OCR_ACTION_DATA_RELEASE:
            {
                ocrGuid_t edtGuid = TRACE_FIELD(TASK, taskDataRelease, trace, taskGuid);
                ocrGuid_t dbGuid = TRACE_FIELD(TASK, taskDataRelease, trace, dbGuid);
                u64 size = TRACE_FIELD(TASK, taskDataRelease, trace, size);
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EDT | ACTION: DB_RELEASE | EDT_GUID: "GUIDF" | DB_GUID: "GUIDF" | DB_SIZE: %lu\n",
                        evt_type[evtType], location, workerId, timestamp, GUIDA(edtGuid), GUIDA(dbGuid), size);

                break;
            }

        }
        break;

    case OCR_TRACE_TYPE_EVENT:

        switch(trace->actionSwitch){

            case OCR_ACTION_CREATE:
            {
                ocrGuid_t parent = TRACE_FIELD(EVENT, eventCreate, trace, parentID);
                genericPrint(evtType, ttype, action, location, workerId, timestamp, parent);
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, workerId, timestamp, NULL_GUID);
                break;

            case OCR_ACTION_SATISFY:
            {
                ocrGuid_t src = TRACE_FIELD(EVENT, eventDepSatisfy, trace, depID);
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EVENT | ACTION: DEP_SATISFY | DEP_GUID: "GUIDF"\n",
                       evt_type[evtType], location, workerId, timestamp, GUIDA(src));
                break;
            }

            case OCR_ACTION_ADD_DEP:
            {
                ocrGuid_t dest = TRACE_FIELD(EVENT, eventDepAdd, trace, depID);
                ocrGuid_t parent = TRACE_FIELD(EVENT, eventDepAdd, trace, parentID);
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: EVENT | ACTION: ADD_DEP | DEP_GUID: "GUIDF" | PARENT: "GUIDF"\n",
                        evt_type[evtType], location, workerId, timestamp, GUIDA(dest), GUIDA(parent));
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
                printf("[TRACE] U/R: %s | PD: 0x%lx | WORKER_ID: %lu | TIMESTAMP: %lu | TYPE: DATABLOCK | ACTION: CREATE | SIZE: %lu | PARENT: "GUIDF"\n",
                        evt_type[evtType], location, workerId, timestamp, size, GUIDA(parent));
                break;
            }
            case OCR_ACTION_DESTROY:
                //No need to grab value from trace object for this trace action yet. Currently only hold no-op placeholders.
                genericPrint(evtType, ttype, action, location, workerId, timestamp, NULL_GUID);
                break;

            default:
                break;
        }
        break;
    }
}

void readFctPtrsFromBinary(char *binaryPath, int lineCount, long *fctPtrs){
    char *sysCommand = malloc(SYS_CALL_COMMAND_LENGTH);
    char path[BIN_PATH_LENGTH];
    strcpy(sysCommand, "nm -nr ");
    strcat(sysCommand, binaryPath);

    FILE *fp = popen(sysCommand, "r");
    free(sysCommand);
    if(fp == NULL){
        //Should not ever happen, as this check is already made in callee.
        printf("Error:  Unable to open application binary\n");
        exit(0);
    }

    int i;
    //Scan over binary file and store function pointer values in an array
    //for faster lookup later
    for(i = 0; i < lineCount; i++){
        long fctPtr;
        if(feof(fp))
            return;
        fscanf(fp, "%lx%*[^\n]", &fctPtr);
        fctPtrs[i] = fctPtr;
    }
}

bool fctPtrExists(long lookup, int lineCount, long *fctPtrs){
    int i;
    //Loop over function pointer array to see if lookup value exists
    for(i = 0; i < lineCount; i++){
        if(fctPtrs[i] == lookup){
            return true;
        }
    }

    return false;

}

int getLineCount(FILE* f){
    if(f == NULL)
        return 0;

    int c = 0;
    int count = 0;
    //Iterate through binary file to get line count for later malloc
    while(!feof(f)){
        c = fgetc(f);
        if(c == '\n'){
            count++;
        }
    }
    return count;
}


