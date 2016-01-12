/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __MAP_SCHEDULER_OBJECT_H__
#define __MAP_SCHEDULER_OBJECT_H__

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_MAP

#include "ocr-scheduler-object.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/hashtable.h"

/****************************************************/
/* OCR MAP SCHEDULER_OBJECT                        */
/****************************************************/

typedef enum {
    OCR_MAP_TYPE_MODULO,
    OCR_MAP_TYPE_MODULO_LOCKED,
} ocrMapType;

typedef struct _paramListSchedulerObjectMap_t {
    paramListSchedulerObject_t base;
    ocrMapType type;
    u32 nbBuckets;
} paramListSchedulerObjectMap_t;

typedef struct _ocrSchedulerObjectMapFcts_t {
    void* (*get)(hashtable_t * hashtable, void * key);
    bool  (*put)(hashtable_t * hashtable, void * key, void * value);
    void* (*tryPut)(hashtable_t * hashtable, void * key, void * value);
    bool  (*remove)(hashtable_t * hashtable, void * key, void ** value);
} ocrSchedulerObjectMapFcts_t;

typedef struct _ocrSchedulerObjectMap_t {
    ocrSchedulerObject_t base;
    ocrMapType type;
    hashtable_t *map;
    ocrSchedulerObjectMapFcts_t mapFcts;
} ocrSchedulerObjectMap_t;

typedef struct _ocrSchedulerObjectMapIterator_t {
    ocrSchedulerObjectIterator_t base;
    ocrSchedulerObject_t *internal;                 /* Internal scheduler object for sanity checking */
    void *key;
} ocrSchedulerObjectMapIterator_t;

/****************************************************/
/* OCR MAP SCHEDULER_OBJECT FACTORY                */
/****************************************************/

typedef struct _ocrSchedulerObjectFactoryMap_t {
    ocrSchedulerObjectFactory_t base;
} ocrSchedulerObjectFactoryMap_t;

typedef struct _paramListSchedulerObjectFactMap_t {
    paramListSchedulerObjectFact_t base;
} paramListSchedulerObjectFactMap_t;

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryMap(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_SCHEDULER_OBJECT_MAP */
#endif /* __MAP_SCHEDULER_OBJECT_H__ */

