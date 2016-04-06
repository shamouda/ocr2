/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __LIST_SCHEDULER_OBJECT_H__
#define __LIST_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_LIST

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/array-list.h"

/****************************************************/
/* OCR LIST SCHEDULER_OBJECT                        */
/****************************************************/

typedef struct _paramListSchedulerObjectList_t {
    paramListSchedulerObject_t base;
    u32 elSize, arrayChunkSize;
    ocrListType type;
} paramListSchedulerObjectList_t;

typedef struct _ocrSchedulerObjectList_t {
    ocrSchedulerObject_t base;
    arrayList_t *list;
} ocrSchedulerObjectList_t;

typedef struct _ocrSchedulerObjectListIterator_t {
    ocrSchedulerObjectIterator_t base;
    ocrSchedulerObject_t *internal;     /* Internal scheduler object for sanity checking. BUG #190 OCR_ASSERT ? */
    slistNode_t *current;               /* List node currently pointed to by the iterator */
} ocrSchedulerObjectListIterator_t;

/****************************************************/
/* OCR LIST SCHEDULER_OBJECT FACTORY                */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryList_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryList_t;

typedef struct _paramListSchedulerObjectFactList_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactList_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryList(ocrParamList_t *perType, u32 factoryId);

/****************************************************/
/* OCR LIST SCHEDULER_OBJECT UTILS                  */
/****************************************************/

void* ocrSchedulerObjectListHead(ocrSchedulerObject_t *self);
void* ocrSchedulerObjectListHeadNext(ocrSchedulerObject_t *self);
void* ocrSchedulerObjectListHeadPlus(ocrSchedulerObject_t *self, u32 index);

#endif /* ENABLE_SCHEDULER_OBJECT_LIST */
#endif /* __LIST_SCHEDULER_OBJECT_H__ */

