/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_MAP

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/map/map-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-MAP SCHEDULER_OBJECT FUNCTIONS                 */
/******************************************************/

static ocrSchedulerObject_t* createSchedulerObjectMapIterator(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectMapIterator_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = OCR_SCHEDULER_OBJECT_MAP_ITERATOR;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)schedObj;
    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key = NULL;
    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = NULL;
    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)schedObj;
    mapIt->map = NULL;
    return schedObj;
}

ocrSchedulerObject_t* mapSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(!paramSchedObj->guidRequired); //No guid required
    if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(paramSchedObj->kind))
        return createSchedulerObjectMapIterator(fact, params);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectMap_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = OCR_SCHEDULER_OBJECT_MAP;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectMap_t* mapSchedObj = (ocrSchedulerObjectMap_t*)schedObj;
    paramListSchedulerObjectMap_t *paramMap = (paramListSchedulerObjectMap_t*)params;
    mapSchedObj->type = paramMap->type;
    switch(paramMap->type) {
    case OCR_MAP_TYPE_MODULO:
        mapSchedObj->map = newHashtableModulo(pd, paramMap->nbBuckets);
        break;
    case OCR_MAP_TYPE_MODULO_LOCKED:
        mapSchedObj->map = newHashtableBucketLockedModulo(pd, paramMap->nbBuckets);
        break;
    default:
        ASSERT(0);
        return NULL;
    }
    return schedObj;
}

u8 mapSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_MAP);
    ocrPolicyDomain_t *pd = fact->pd;
    if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(self->kind)) {
        pd->fcts.pdFree(pd, self);
        return 0;
    }
    ocrSchedulerObjectMap_t* mapSchedObj = (ocrSchedulerObjectMap_t*)self;
    switch(mapSchedObj->type) {
    case OCR_MAP_TYPE_MODULO:
        destructHashtable(mapSchedObj->map, NULL);
        break;
    case OCR_MAP_TYPE_MODULO_LOCKED:
        destructHashtableBucketLocked(mapSchedObj->map, NULL);
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 mapSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_MAP);
    ASSERT(element->kind == OCR_SCHEDULER_OBJECT_MAP_ITERATOR);
    ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)element;
    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
    ocrSchedulerObjectMap_t *schedObj = (ocrSchedulerObjectMap_t*)self;
    ASSERT(mapIt->map == schedObj->map);
    switch(schedObj->type) {
    case OCR_MAP_TYPE_MODULO: {
            switch(properties) {
            case SCHEDULER_OBJECT_INSERT_MAP_PUT:
                hashtableNonConcPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            case SCHEDULER_OBJECT_INSERT_MAP_TRY_PUT:
                it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableNonConcTryPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            case SCHEDULER_OBJECT_INSERT_MAP_CONC_PUT:
                hashtableConcPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            case SCHEDULER_OBJECT_INSERT_MAP_CONC_TRY_PUT:
                it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableConcTryPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    case OCR_MAP_TYPE_MODULO_LOCKED: {
            switch(properties) {
            case SCHEDULER_OBJECT_INSERT_MAP_PUT:
            case SCHEDULER_OBJECT_INSERT_MAP_CONC_PUT:
                hashtableConcBucketLockedPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            case SCHEDULER_OBJECT_INSERT_MAP_TRY_PUT:
            case SCHEDULER_OBJECT_INSERT_MAP_CONC_TRY_PUT:
                it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableConcBucketLockedTryPut(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 mapSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_MAP);
    ASSERT(element && element->kind == OCR_SCHEDULER_OBJECT_MAP_ITERATOR);
    ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)element;
    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
    ocrSchedulerObjectMap_t *schedObj = (ocrSchedulerObjectMap_t*)self;
    ASSERT(mapIt->map == schedObj->map);
    switch(schedObj->type) {
    case OCR_MAP_TYPE_MODULO: {
            switch(properties) {
            case SCHEDULER_OBJECT_REMOVE_MAP_NON_CONC:
                hashtableNonConcRemove(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, &it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            case SCHEDULER_OBJECT_REMOVE_MAP_CONC:
                hashtableConcRemove(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, &it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    case OCR_MAP_TYPE_MODULO_LOCKED: {
            switch(properties) {
            case SCHEDULER_OBJECT_REMOVE_MAP_NON_CONC:
            case SCHEDULER_OBJECT_REMOVE_MAP_CONC:
                hashtableConcBucketLockedRemove(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key, &it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value);
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }

    if (dst) {
        if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(dst->kind) && dst != element) {
            ASSERT(SCHEDULER_OBJECT_KIND(dst->kind) == OCR_SCHEDULER_OBJECT_MAP);
            ocrSchedulerObjectIterator_t *dstIt = (ocrSchedulerObjectIterator_t*)dst;
            dstIt->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value;
            ocrSchedulerObjectMapIterator_t *dstMapIt = (ocrSchedulerObjectMapIterator_t*)dst;
            dstMapIt->map = schedObj->map;
        } else {
            ASSERT(dst->kind == OCR_SCHEDULER_OBJECT_VOID_PTR);
            dst->guid.metaDataPtr = it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value;
        }
    }
    return 0;
}

u8 mapSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_MAP);
    ASSERT(iterator->base.kind == OCR_SCHEDULER_OBJECT_MAP_ITERATOR);
    ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)iterator;
    ocrSchedulerObjectMap_t *schedObj = (ocrSchedulerObjectMap_t*)self;
    switch(schedObj->type) {
    case OCR_MAP_TYPE_MODULO: {
            switch(properties) {
            case SCHEDULER_OBJECT_ITERATE_BEGIN:
                {
                    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
                    mapIt->map = schedObj->map;
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key = NULL;
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = NULL;
                }
                break;
            case SCHEDULER_OBJECT_ITERATE_MAP_GET_NON_CONC:
                {
                    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
                    ASSERT(mapIt->map == schedObj->map);
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableNonConcGet(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key);
                }
                break;
            case SCHEDULER_OBJECT_ITERATE_MAP_GET_CONC:
                {
                    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
                    ASSERT(mapIt->map == schedObj->map);
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableConcGet(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key);
                }
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    case OCR_MAP_TYPE_MODULO_LOCKED: {
            switch(properties) {
            case SCHEDULER_OBJECT_ITERATE_BEGIN:
                {
                    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
                    mapIt->map = schedObj->map;
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key = NULL;
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = NULL;
                }
                break;
            case SCHEDULER_OBJECT_ITERATE_MAP_GET_NON_CONC:
            case SCHEDULER_OBJECT_ITERATE_MAP_GET_CONC:
                {
                    ocrSchedulerObjectMapIterator_t *mapIt = (ocrSchedulerObjectMapIterator_t*)it;
                    ASSERT(mapIt->map == schedObj->map);
                    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).value = hashtableConcBucketLockedGet(schedObj->map, it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_MAP).key);
                }
                break;
            default:
                ASSERT(0);
                return OCR_ENOTSUP;
            }
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u64 mapSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

ocrSchedulerObject_t* mapGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 mapSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-MAP SCHEDULER_OBJECT FACTORY FUNCTIONS        */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectMap(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryMap(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryMap(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ASSERT(paramFact->kind == OCR_SCHEDULER_OBJECT_AGGREGATE);
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryMap_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectMap_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_MAP;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectMap;
    schedObjFact->destruct = &destructSchedulerObjectFactoryMap;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), mapSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), mapSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), mapSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), mapSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), mapSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), mapSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), mapSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), mapGetSchedulerObjectForLocation);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_MAP */
