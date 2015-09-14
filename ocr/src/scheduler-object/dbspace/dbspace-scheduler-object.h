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
    u64 dbSize;
    void *dataPtr;
} paramListSchedulerObjectDbspace_t;

typedef struct _ocrSchedulerObjectDbspace_t {
    ocrSchedulerObject_t base;
    ocrGuid_t dbGuid;                               //Guid of the DB
    u64 dbSize;                                     //Size of the DB
    void *dataPtr;                                  //Data pointer for the DB
    u64 time;                                       //Time frontier on this DB node
    volatile u32 lock;                              //Lock for this scheduler object
    ocrSchedulerObject_t *dbTimeList;               //Phases when this DB will be used
    ocrSchedulerObjectIterator_t *listIterator;		//Preallocated dbTime list iterator
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

