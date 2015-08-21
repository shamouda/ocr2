#include "ocr-config.h"
#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include <stdarg.h>

/*
 * Data structure for trace objects.  Not yet fully populated
 * as not all trace coverage is supported yet.
 *
 */

typedef struct _ocrTraceObj_t {

    u64 time;               /*Timestamp for event*/
    u64 location;           /*PD where event occured*/
    bool isUserType;        /* 1 if object was created by user 0 for runtime */
    unsigned char **blob;   /* TODO Carry generic blob*/

    union{ /*args*/

        struct{ /* Task (EDT) */
            union{
                struct{
                    ocrGuid_t parentID;             /*GUID of parent creating cur EDT*/
                }taskCreate;

                struct{
                    ocrGuid_t depID;               /*GUIDs of dependant objects */
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

            }action;

        }task;

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

        }data;

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
                    void *placeHolder;              /* future TODO Define values.  What trigger?*/
                }eventTrigger;

                struct{
                    void *placeHolder;              /* future TODO Define values. may not be needed*/
                }eventDestroy;

            }action;

        }event;

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

        }executionUnit;

        struct{ /* User-facing custom marker */
            struct{
                void *placeHolder;              /* future TODO Define user facing options*/
            }userMarkerFlags;

        }userMarker;


        struct{ /* Runtime facing custom Marker */
            struct{
                void *placeHolder;              /* future TODO define runtime options*/
            }runtimeMarkerFlags;

        }runtimeMarker;


    }type;

} ocrTraceObj_t;

void doTrace(u64 location, u64 wrkr, ocrGuid_t taskGuid, char *str, ...);
u32 numVarsInString(char *fmt);
void populateTraceObject(bool isUserType, ocrTraceType_t objType, ocrTraceAction_t actionType, u64 location, u64 timestamp, ocrGuid_t parent, va_list ap);
static void genericPrint(bool isUserType, ocrTraceType_t trace, ocrTraceAction_t action, u64 location, u64 timestamp, ocrGuid_t parent);
