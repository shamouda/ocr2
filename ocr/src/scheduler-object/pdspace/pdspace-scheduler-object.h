/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PDSPACE_SCHEDULER_OBJECT_H__
#define __PDSPACE_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR PDSPACE SCHEDULER_OBJECT                     */
/****************************************************/

typedef struct _paramListSchedulerObjectPdspace_t {
    paramListSchedulerObject_t base;
} paramListSchedulerObjectPdspace_t;

typedef struct _ocrSchedulerObjectPdspace_t {
    ocrSchedulerObject_t base;
    ocrSchedulerObject_t *dbMap;    /* Hash table to map a DB guid to the scheduler object in this node */
    ocrSchedulerObject_t *wst;      /* Scheduler object for a workstealing place */
    volatile u32 lock;
} ocrSchedulerObjectPdspace_t;

/****************************************************/
/* OCR PDSPACE SCHEDULER_OBJECT FACTORY             */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryPdspace_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryPdspace_t;

typedef struct _paramListSchedulerObjectFactPdspace_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactPdspace_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryPdspace(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_PDSPACE */
#endif /* __PDSPACE_SCHEDULER_OBJECT_H__ */

