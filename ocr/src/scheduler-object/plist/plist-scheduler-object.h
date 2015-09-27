/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __PLIST_SCHEDULER_OBJECT_H__
#define __PLIST_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_PLIST

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/array-list.h"

/****************************************************/
/* OCR PLIST SCHEDULER_OBJECT                        */
/****************************************************/

typedef struct _paramListSchedulerObjectPlist_t {
    paramListSchedulerObject_t base;
    u32 elSize, arrayChunkSize;
    ocrListType type;
} paramListSchedulerObjectPlist_t;

typedef struct _ocrSchedulerObjectPlist_t {
    ocrSchedulerObject_t base;
    arrayList_t *plist;
    volatile u32 lock;
} ocrSchedulerObjectPlist_t;

typedef struct _ocrSchedulerObjectPlistIterator_t {
    ocrSchedulerObjectIterator_t base;
    slistNode_t *current;				/* Plist node currently pointed to by the iterator */
} ocrSchedulerObjectPlistIterator_t;

/****************************************************/
/* OCR PLIST SCHEDULER_OBJECT FACTORY                */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryPlist_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryPlist_t;

typedef struct _paramListSchedulerObjectFactPlist_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactPlist_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryPlist(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_PLIST */
#endif /* __PLIST_SCHEDULER_OBJECT_H__ */

