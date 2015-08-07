/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DBNODE_SCHEDULER_OBJECT_H__
#define __DBNODE_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DBNODE

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR DBNODE SCHEDULER_OBJECT                      */
/****************************************************/

typedef struct _paramListSchedulerObjectDbNode_t {
    paramListSchedulerObject_t base;
    u64 initialPhase;
    u64 dbSize;
    void *dataPtr;
} paramListSchedulerObjectDbNode_t;

typedef enum {
    DBNODE_EDT_STATE_WAITING,           //EDT is waiting in a future phase waitList
    DBNODE_EDT_STATE_PRESCRIBED,        //EDT is in current phase and is prescribed an initial schedule cycle but its final schedule cycle is yet unknown
    DBNODE_EDT_STATE_RESOLVED,          //EDT's final schedule cycle has been resolved and activated waiting to be released for execution
    DBNODE_EDT_STATE_ACTIVE,            //EDT has been released for execution
} ocrDbNodeEdtState;

typedef struct _listDataEdt_t {
    ocrFatGuid_t edt;                   //EDT guid and metaDataPtr
    ocrDbAccessMode_t mode;             //Access mode of this EDT for this DB
    ocrDbNodeEdtState state;            //Current state of the EDT
    u64 scheduleCycle;                  //Schedule cycle when this EDT will be released for execution
} listDataEdt_t;

typedef struct _listDataPhase_t {
    u64 phase;                          //Phase of the DB
    ocrLocation_t loc;                  //Location to where this DB is mapped during that phase
    ocrSchedulerObject_t *waitList;     //EDTs that are scheduled for execution on that phase
} listDataPhase_t;

typedef struct _ocrSchedulerObjectDbNode_t {
    ocrSchedulerObject_t base;
    ocrSchedulerObject_t *phaseList;    //Phases when this DB will be used
    ocrSchedulerObject_t *activeList;   //EDTs activated in current phase that will be ready to execute after their access mode conflicts are resolved
    u64 executionClock;                 //Latest logical clock cycle on the DB for EDTs that have been released for execution
    u64 scheduleClock;                  //Latest logical clock cycle on the DB for EDTs that are scheduled to be released in future
    u64 currentPhase;                   //Current phase of this DB node
    u64 currentLoc;                     //Current location of this DB node
    u64 dbSize;                         //Size of the DB
    void *dataPtr;                      //Data pointer for the DB
    volatile u32 lock;                  //Lock for this scheduler object
} ocrSchedulerObjectDbNode_t;

/****************************************************/
/* OCR DBNODE SCHEDULER_OBJECT FACTORY              */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryDbNode_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryDbNode_t;

typedef struct _paramListSchedulerObjectFactDbNode_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactDbNode_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDbNode(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_DBNODE */
#endif /* __DBNODE_SCHEDULER_OBJECT_H__  */

