/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __CTQ_SCHEDULER_OBJECT_H__
#define __CTQ_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_CTQ

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/******************************************************/
/* OCR ABSTRACT COMPOSITE TASK QUEUE SCHEDULER_OBJECT */
/******************************************************/

typedef struct _paramListSchedulerObjectCtq_t {
    paramListSchedulerObject_t base;
} paramListSchedulerObjectCtq_t;

typedef struct _ocrSchedulerObjectCtq_t {
    ocrSchedulerObject_t base;
    ocrSchedulerObjectFactory_t *subfactory;
} ocrSchedulerObjectCtq_t;

/****************************************************/
/* OCR CTQ SCHEDULER_OBJECT FACTORY                 */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryCtq_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryCtq_t;

typedef struct _paramListSchedulerObjectFactCtq_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactCtq_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryCtq(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_CTQ */
#endif /* __CTQ_SCHEDULER_OBJECT_H__ */

