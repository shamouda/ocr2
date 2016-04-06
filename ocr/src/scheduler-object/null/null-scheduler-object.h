/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __NULL_SCHEDULER_OBJECT_H__
#define __NULL_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_NULL

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* OCR NULL SCHEDULER_OBJECT                        */
/****************************************************/

typedef struct _paramListSchedulerObjectNull_t {
    paramListSchedulerObject_t base;
} paramListSchedulerObjectNull_t;

typedef struct _ocrSchedulerObjectNull_t {
    ocrSchedulerObject_t base;
} ocrSchedulerObjectNull_t;

/****************************************************/
/* OCR NULL SCHEDULER_OBJECT FACTORY                */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryNull_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryNull_t;

typedef struct _paramListSchedulerObjectFactNull_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactNull_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryNull(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_NULL */
#endif /* __NULL_SCHEDULER_OBJECT_H__ */

