/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __BIN_HEAP_SCHEDULER_OBJECT_H__
#define __BIN_HEAP_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/bin-heap.h"

/****************************************************/
/* OCR BIN_HEAP SCHEDULER_OBJECT                         */
/****************************************************/

typedef struct _paramListSchedulerObjectBinHeap_t {
    paramListSchedulerObject_t base;
    ocrBinHeapType_t type;
} paramListSchedulerObjectBinHeap_t;

typedef struct _ocrSchedulerObjectBinHeap_t {
    ocrSchedulerObject_t base;
    binHeap_t *binHeap;             // the binHeap
    ocrBinHeapType_t binHeapType;   // type of binHeap
} ocrSchedulerObjectBinHeap_t;

/****************************************************/
/* OCR BIN_HEAP SCHEDULER_OBJECT FACTORY                 */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryBinHeap_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryBinHeap_t;

typedef struct _paramListSchedulerObjectFactBinHeap_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactBinHeap_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryBinHeap(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_BIN_HEAP */
#endif /* __BIN_HEAP_SCHEDULER_OBJECT_H__ */

