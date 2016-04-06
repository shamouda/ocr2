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
} paramListSchedulerObjectDbtime_t;

// A DB time object keeps track of a specific time slot
// in the DB's timeline on this PD. EDTs are scheduled
// to access DBs according to these time slots.
typedef struct _ocrSchedulerObjectDbtime_t {
    ocrSchedulerObject_t base;
    ocrLocation_t space;                            //Scheduled location of DB (updated only on scheduler nodes)
                                                    // - A scheduling node maintains DbTime objects for all locations of the DB
                                                    // - On a non-scheduling node this is always current PD location
    u64 time;                                       //Scheduler reference global time tick
    ocrSchedulerObject_t *waitList;                 //List of EDTs that are waiting either for DB data
    ocrSchedulerObject_t *readyList;                //List of EDTs that are ready to acquire the DB
    u32 exclusiveWaiterCount;                       //Number of EDTs that are waiting to acquire DB in exclusive mode
    u32 edtScheduledCount;                          //Total number of EDTs overall that have been scheduled to access this DB for this time slot
    u32 edtDoneCount;                               //Total number of EDTs that have completed execution in this time slot
    u32 schedulerCount;                             //Count kept by scheduler node for number of EDTs scheduled
    bool schedulerDone;                             //Flag used to identify if this time slot is complete at scheduler node
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

