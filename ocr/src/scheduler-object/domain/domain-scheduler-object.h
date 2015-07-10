/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DOMAIN_SCHEDULER_OBJECT_H__
#define __DOMAIN_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DOMAIN

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR DOMAIN SCHEDULER_OBJECT                                */
/****************************************************/

typedef struct _paramListSchedulerObjectDomain_t {
    paramListSchedulerObject_t base;
} paramListSchedulerObjectDomain_t;

typedef struct _ocrSchedulerObjectDomain_t {
    ocrSchedulerObject_t base;
    ocrSchedulerObject_t *dbTable;  /* Hash table to map a DB guid to the scheduler object in this node */
    ocrSchedulerObject_t *wst;      /* Scheduler object for a workstealing place */
} ocrSchedulerObjectDomain_t;

/****************************************************/
/* OCR DOMAIN SCHEDULER_OBJECT FACTORY                        */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryDomain_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryDomain_t;

typedef struct _ocrSchedulerObjectRootFactoryDomain_t {
    ocrSchedulerObjectRootFactory_t base;
} ocrSchedulerObjectRootFactoryDomain_t;

typedef struct _paramListSchedulerObjectFactDomain_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactDomain_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDomain(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_DOMAIN */
#endif /* __DOMAIN_SCHEDULER_OBJECT_H__ */

