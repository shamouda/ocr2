/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PR_WSH_SCHEDULER_OBJECT_H__
#define __PR_WSH_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR PR_WSH SCHEDULER_OBJECT                      */
/* (priority work-sharing scheduler)                */
/****************************************************/

typedef struct _paramListSchedulerObjectPrWsh_t {
    paramListSchedulerObject_t base;
} paramListSchedulerObjectPrWsh_t;

typedef struct _ocrSchedulerObjectPrWsh_t {
    ocrSchedulerObject_t base;
    ocrSchedulerObject_t *heap;    /* priority queue */
} ocrSchedulerObjectPrWsh_t;

/****************************************************/
/* OCR PR_WSH SCHEDULER_OBJECT FACTORY              */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryPrWsh_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryPrWsh_t;

typedef struct _paramListSchedulerObjectFactPrWsh_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactPrWsh_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryPrWsh(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_PR_WSH */
#endif /* __PR_WSH_SCHEDULER_OBJECT_H__ */

