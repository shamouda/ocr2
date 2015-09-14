/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DBTIME_SCHEDULER_OBJECT_H__
#define __DBTIME_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR DBTIME SCHEDULER_OBJECT                      */
/****************************************************/

typedef struct _paramListSchedulerObjectDbtime_t {
    paramListSchedulerObject_t base;
    ocrLocation_t space;
    u64 time;
    ocrDbAccessMode_t mode;
} paramListSchedulerObjectDbtime_t;

typedef struct _ocrSchedulerObjectDbtime_t {
    ocrSchedulerObject_t base;
    ocrLocation_t space;                //Current location of DB (updated only on scheduler nodes)
                                        // - On a non-scheduling node this is always current PD location
                                        // - A scheduling node maintains DbTime objects for all locations of the DB
    u64 time;                           //Scheduler reference global time tick
    ocrDbAccessMode_t mode;             //Max access mode according to mode lattice
    bool dbRequestSent;                 //Flag to indicate if request to fetch DB has been sent
    ocrSchedulerObject_t *edtList;      //List of EDTs scheduled to access this DB at this time (and in space)
                                        // - Only maintained on current nodes, i.e. scheduling nodes don’t maintain
                                        //   EDT lists for other locations for which it has DbTime nodes
    u32 edtSchedCount;                  //Number of EDTs scheduled for this time slice for this DB
    u32 edtDoneCount;                   //Number of scheduled EDTs that have completed execution
    volatile u32 lock;                  //Lock for this scheduler object
} ocrSchedulerObjectDbtime_t;

/****************************************************/
/* OCR DBTIME SCHEDULER_OBJECT FACTORY              */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryDbtime_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryDbtime_t;

typedef struct _paramListSchedulerObjectFactDbtime_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactDbtime_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDbtime(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_DBTIME */
#endif /* __DBTIME_SCHEDULER_OBJECT_H__  */

