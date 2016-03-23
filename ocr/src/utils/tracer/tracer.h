#ifndef __TRACER_H__
#define __TRACER_H__

#include "ocr-config.h"
#include "ocr-runtime-types.h"
#include "ocr-types.h"

#ifdef ENABLE_WORKER_SYSTEM

#include <stdarg.h>

/*
 * Data structure for trace objects.  Not yet fully populated
 * as not all trace coverage is needed/supported yet.
 *
 */
#define TRACE_TYPE_NAME(ID) _type_##ID


#define _TRACE_FIELD_FULL(ttype, taction, obj, field) obj->type._type_##ttype.action.taction.field
#define TRACE_FIELD(type, action, traceObj, field) _TRACE_FIELD_FULL(type, action, (traceObj), field)

typedef struct {

    ocrTraceType_t  typeSwitch;       //TODO: Should (maybe) be accessed through a macro in the future.
    ocrTraceAction_t actionSwitch;    //

    u64 time;               /*Timestamp for event*/
    u64 workerId;           /*Worker where event occured*/
    u64 location;           /*PD where event occured*/
    bool eventType;         /* TODO make this more descriptive than bool*/
    unsigned char **blob;   /* TODO Carry generic blob*/

    union{ /*type*/

        struct{ /* Task (EDT) */
            union{
                struct{
                    ocrGuid_t parentID;             /*GUID of parent creating cur EDT*/
                }taskCreate;

                struct{
                    ocrGuid_t depID;               /*GUIDs of dependent objects */
                    u32 parentPermissions;          /*Parent permissions*/
                }taskDepReady;

                struct{
                    ocrGuid_t depID;
                }taskDepSatisfy;

                struct{
                    u32 whyReady;                   /*Last Satisfyee??*/
                }taskReadyToRun;
                struct{
                    u32 whyDelay;                   /* future TODO define this... may not be needed/useful*/
                    ocrEdt_t funcPtr;                   /*Executing function*/
                }taskExeBegin;

                struct{
                    void *placeHolder;              /* future TODO: define useful fields*/
                }taskExeEnd;

                struct{
                    void *placeHolder;              /* future TODO: define useful fields*/
                }taskDestroy;

                struct{
                    ocrGuid_t taskGuid;             /* EDT doing the acquire */
                    ocrGuid_t dbGuid;               /* Datablock being acquired */
                    u64 size;                       /* Size of Datablock being acquired */
                }taskDataAcquire;

                struct{
                    ocrGuid_t taskGuid;             /* EDT doing the release */
                    ocrGuid_t dbGuid;               /* Datablock being released */
                    u64 size;                       /* Size of Datablock being released */
                }taskDataRelease;

            }action;

        } TRACE_TYPE_NAME(TASK);

        struct{ /* Data (DB) */
            union{
                struct{
                    ocrLocation_t location;         /*Location where created*/
                    ocrGuid_t parentID;             /*GUID of parent creating cur DB*/
                    u64 size;                       /*size of DB in bytes*/
                }dataCreate;

                struct{
                    void *memID;                    /* future TODO define type for memory ID*/
                }dataSize;

                struct{
                    ocrLocation_t src;              /*Data source location*/
                }dataMoveFrom;

                struct{
                    ocrLocation_t dest;             /*Data destination location*/
                }dataMoveTo;

                struct{
                    ocrGuid_t duplicateID;          /*GUID of new DB when copied*/
                }dataReplicate;

                struct{
                    void* placeHolder;              /* future TODO define this.  may not be needed*/
                }dataDestroy;

            }action;

        } TRACE_TYPE_NAME(DATA);

        struct{ /* Event (OCR module) */
            union{
                struct{
                    ocrGuid_t parentID;             /*GUID of parent creating current Event*/
                }eventCreate;

                struct{
                    ocrGuid_t depID;                /*GUIDs of dependent OCR object*/
                    ocrGuid_t parentID;             /*GUIDs of parents (needed?)*/
                }eventDepAdd;

                struct{
                    ocrGuid_t depID;                /*GUID responsible for satisfaction*/
                }eventDepSatisfy;

                struct{
                    void *placeHolder;              /* future TODO Define values.  What trigger?*/
                }eventTrigger;

                struct{
                    void *placeHolder;              /* future TODO Define values. may not be needed*/
                }eventDestroy;

            }action;

        } TRACE_TYPE_NAME(EVENT);

        struct{ /* Execution Unit (workers) */
            union{
                struct{
                    ocrLocation_t location;         /*Location worker belongs to (PD)*/
                }exeUnitStart;

                struct{
                    ocrLocation_t location;         /*Location after work shift*/
                }exeUnitMigrate;

                struct{
                    void *placeHolder;              /* future TODO Define values.  May not be needed*/
                }exeUnitDestroy;

            }action;

        } TRACE_TYPE_NAME(EXECUTION_UNIT);

        struct{ /* User-facing custom marker */
            union{
                struct{
                    void *placeHolder;              /* future TODO Define user facing options*/
                }userMarkerFlags;

            }action;

        } TRACE_TYPE_NAME(USER_MARKER);


        struct{ /* Runtime facing custom Marker */
            union{
                struct{
                    void *placeHolder;              /* future TODO define runtime options*/
                }runtimeMarkerFlags;

            }action;

        } TRACE_TYPE_NAME(RUNTIME_MARKER);


    }type;
}ocrTraceObj_t;

#endif /* ENABLE_WORKER_SYSTEM */
void doTrace(u64 location, u64 wrkr, ocrGuid_t taskGuid, ...);

#endif
