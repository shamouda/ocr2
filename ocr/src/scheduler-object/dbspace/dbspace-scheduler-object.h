/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DBSPACE_SCHEDULER_OBJECT_H__
#define __DBSPACE_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR DBSPACE SCHEDULER_OBJECT                     */
/****************************************************/

typedef struct _paramListSchedulerObjectDbspace_t {
    paramListSchedulerObject_t base;
    ocrGuid_t dbGuid;
} paramListSchedulerObjectDbspace_t;

//This shows the various states of a DB object in a PD.
typedef enum _ocrDbState {
    DB_STATE_PROXY,             // DB object just knows DB guid (no size or any other info. May/may not have EDTs waiting to acquire it).
    DB_STATE_INFO,              // DB object has guid + size + all relevant info. (no data available in PD and no EDTs waiting to acquire it).
    DB_STATE_LOCAL_INACTIVE,    // DB object has info + data locally acquired by PD. No EDTs are accessing or waiting.
    DB_STATE_LOCAL_ACTIVE,      // DB object has data locally acquired by PD + EDTs are actively accessing it or waiting to access.
    DB_STATE_REMOTE_INACTIVE,   // DB object has info + EDTs are waiting on it but data is not locally acquired in PD.
} ocrDbState;

//BUG #920 Cleanup:
typedef enum _ocrDbAcquireMode {
    DB_ACQUIRE_NONE,
    DB_ACQUIRE_EXCLUSIVE,
    DB_ACQUIRE_SHARED,
} ocrDbAcquireMode;

// The DB space object manages the scheduler state of a DB
// in a PD. It keeps track of a timeline for EDTs to access
// the DB in a node.
typedef struct _ocrSchedulerObjectDbspace_t {
    ocrSchedulerObject_t base;
    ocrGuid_t dbGuid;                               //Guid of the DB
    u64 dbSize;                                     //Size of the DB
    void *dbPtr;                                    //Data pointer for the DB (BUG #920 Cleanup)
    ocrDbState state;                               //Current state of the DB
    u64 time;                                       //Time where DB is currently located
    volatile u32 lock;                              //Lock for this scheduler object
    u64 activeCount;                                //Number of EDTs actively accessing this DB
    ocrDbAcquireMode mode;                          //Current mode of acquire of the DB (BUG #920 Cleanup)
    bool free;                                      //Flag that indicates that DB is marked to be freed
    ocrSchedulerObject_t *dbTimeList;               //List of time slots when this DB will be accessed by EDTs
    ocrSchedulerObject_t *schedList;                //List of EDTs that are waiting to be scheduled
                                                    // - handles case when DB and EDT messages are not received at scheduler node in order
    ocrSchedulerObjectIterator_t *listIterator;     //Preallocated list iterator used by scheduling node
} ocrSchedulerObjectDbspace_t;

/****************************************************/
/* OCR DBSPACE SCHEDULER_OBJECT FACTORY             */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryDbspace_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryDbspace_t;

typedef struct _paramListSchedulerObjectFactDbspace_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactDbspace_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDbspace(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_DBSPACE */
#endif /* __DBSPACE_SCHEDULER_OBJECT_H__  */

