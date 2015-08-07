/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_LIST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/list/list-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-LIST SCHEDULER_OBJECT FUNCTIONS                */
/******************************************************/

static ocrSchedulerObject_t* createSchedulerObjectListIterator(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectListIterator_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = OCR_SCHEDULER_OBJECT_LIST_ITERATOR;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)schedObj;
    it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_LIST).el = NULL;
    ocrSchedulerObjectListIterator_t *listIt = (ocrSchedulerObjectListIterator_t*)schedObj;
    listIt->list = NULL;
    listIt->current = NULL;
    return schedObj;
}

ocrSchedulerObject_t* listSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    ASSERT(!paramSchedObj->guidRequired); //No guid required
    if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(paramSchedObj->kind))
        return createSchedulerObjectListIterator(fact, params);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectList_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = OCR_SCHEDULER_OBJECT_LIST;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = INVALID_LOCATION;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectList_t* listSchedObj = (ocrSchedulerObjectList_t*)schedObj;
    paramListSchedulerObjectList_t *paramList = (paramListSchedulerObjectList_t*)params;
    listSchedObj->list = newArrayList(paramList->elSize, paramList->arrayChunkSize, paramList->type);
    return schedObj;
}

u8 listSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_LIST);
    ocrPolicyDomain_t *pd = fact->pd;
    if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(self->kind)) {
        pd->fcts.pdFree(pd, self);
        return 0;
    }
    ocrSchedulerObjectList_t* listSchedObj = (ocrSchedulerObjectList_t*)self;
    destructArrayList(listSchedObj->list);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 listSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_LIST);
    ocrSchedulerObjectList_t *schedObj = (ocrSchedulerObjectList_t*)self;
    if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(element->kind)) {
        ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)element;
        void *data = it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_LIST).el;
        ocrSchedulerObjectListIterator_t *lit = (ocrSchedulerObjectListIterator_t*)element;
        switch(properties) {
        case SCHEDULER_OBJECT_INSERT_LIST_FRONT: {
                lit->list = schedObj->list;
                lit->current = newArrayListNodeBefore(schedObj->list, schedObj->list->head);
                if (data) hal_memCopy(lit->current->data, data, schedObj->list->elSize, 0);
            }
            break;
        case SCHEDULER_OBJECT_INSERT_LIST_BACK: {
                lit->list = schedObj->list;
                lit->current = newArrayListNodeAfter(schedObj->list, schedObj->list->tail);
                if (data) hal_memCopy(lit->current->data, data, schedObj->list->elSize, 0);
            }
            break;
        case SCHEDULER_OBJECT_INSERT_LIST_BEFORE: {
                ASSERT(lit->list == schedObj->list);
                lit->current = newArrayListNodeBefore(schedObj->list, lit->current);
                if (data) hal_memCopy(lit->current->data, data, schedObj->list->elSize, 0);
            }
            break;
        case SCHEDULER_OBJECT_INSERT_LIST_AFTER: {
                ASSERT(lit->list == schedObj->list);
                lit->current = newArrayListNodeAfter(schedObj->list, lit->current);
                if (data) hal_memCopy(lit->current->data, data, schedObj->list->elSize, 0);
            }
        default:
            ASSERT(0);
            return OCR_ENOTSUP;
        }
        it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_LIST).el = lit->current->data;
    } else {
        ASSERT(element->kind == OCR_SCHEDULER_OBJECT_VOID_PTR);
        void *data = element->guid.metaDataPtr;
        switch(properties) {
        case SCHEDULER_OBJECT_INSERT_LIST_FRONT:
            data = pushFrontArrayList(schedObj->list, data);
            break;
        case SCHEDULER_OBJECT_INSERT_LIST_BACK:
            data = pushBackArrayList(schedObj->list, data);
            break;
        default:
            ASSERT(0);
            return OCR_ENOTSUP;
        }
        element->guid.metaDataPtr = data;
    }
    return 0;
}

u8 listSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_LIST);
    ocrSchedulerObjectList_t *schedObj = (ocrSchedulerObjectList_t*)self;

    void *data = NULL;
    if (dst) {
        if (IS_SCHEDULER_OBJECT_TYPE_ITERATOR(dst->kind)) {
            ASSERT(SCHEDULER_OBJECT_KIND(dst->kind) == OCR_SCHEDULER_OBJECT_LIST);
            ocrSchedulerObjectIterator_t *it = (ocrSchedulerObjectIterator_t*)dst;
            data = it->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_LIST).el;
        } else {
            ASSERT(dst->kind == OCR_SCHEDULER_OBJECT_VOID_PTR);
            data = dst->guid.metaDataPtr;
        }
    }

    switch(properties) {
    case SCHEDULER_OBJECT_REMOVE_LIST_FRONT: {
            ASSERT(schedObj->list->head);
            if (data) hal_memCopy(data, schedObj->list->head->data, schedObj->list->elSize, 0);
            popFrontArrayList(schedObj->list);
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_LIST_BACK: {
            ASSERT(schedObj->list->tail);
            if (data) hal_memCopy(data, schedObj->list->tail->data, schedObj->list->elSize, 0);
            popBackArrayList(schedObj->list);
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_LIST_CURRENT: {
            ASSERT(element && element->kind == OCR_SCHEDULER_OBJECT_LIST_ITERATOR);
            ocrSchedulerObjectListIterator_t *lit = (ocrSchedulerObjectListIterator_t*)element;
            ASSERT(lit->list == schedObj->list && lit->current);
            if (data) hal_memCopy(data, lit->current->data, schedObj->list->elSize, 0);
            freeArrayListNode(schedObj->list, lit->current);
            lit->current = NULL;
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_LIST_BEFORE: {
            ASSERT(element && element->kind == OCR_SCHEDULER_OBJECT_LIST_ITERATOR);
            ocrSchedulerObjectListIterator_t *lit = (ocrSchedulerObjectListIterator_t*)element;
            ASSERT(lit->list == schedObj->list && lit->current);
            ASSERT(lit->list->type == OCR_LIST_TYPE_DOUBLE); //TODO: Enable for single too
            slistNode_t *node = ((dlistNode_t*)lit->current)->prev;
            ASSERT(node);
            if (data) hal_memCopy(data, node->data, schedObj->list->elSize, 0);
            freeArrayListNode(schedObj->list, node);
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_LIST_AFTER: {
            ASSERT(element && element->kind == OCR_SCHEDULER_OBJECT_LIST_ITERATOR);
            ocrSchedulerObjectListIterator_t *lit = (ocrSchedulerObjectListIterator_t*)element;
            ASSERT(lit->list == schedObj->list && lit->current);
            slistNode_t *node = lit->current->next;
            ASSERT(node);
            if (data) hal_memCopy(data, node->data, schedObj->list->elSize, 0);
            freeArrayListNode(schedObj->list, node);
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    return 0;
}

u8 listSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(SCHEDULER_OBJECT_KIND(self->kind) == OCR_SCHEDULER_OBJECT_LIST);
    ASSERT(iterator->base.kind == OCR_SCHEDULER_OBJECT_LIST_ITERATOR);
    ocrSchedulerObjectList_t *schedObj = (ocrSchedulerObjectList_t*)self;
    ocrSchedulerObjectListIterator_t *listIterator = (ocrSchedulerObjectListIterator_t*)iterator;
    if(listIterator->list != schedObj->list) {
        listIterator->list = schedObj->list;
        listIterator->current = schedObj->list->head;
    }
    void *data = NULL;
    switch(properties) {
    case SCHEDULER_OBJECT_ITERATE_BEGIN: {
            listIterator->current = listIterator->list->head;
            data = listIterator->current ? listIterator->current->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_END: {
            listIterator->current = listIterator->list->tail;
            data = listIterator->current ? listIterator->current->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_CURRENT: {
            data = listIterator->current ? listIterator->current->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_NEXT: {
            if (listIterator->current) listIterator->current = listIterator->current->next;
            data = listIterator->current ? listIterator->current->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_PREV: {
            ASSERT(listIterator->list->type == OCR_LIST_TYPE_DOUBLE); //TODO: Enable for single too
            if (listIterator->current) listIterator->current = ((dlistNode_t*)listIterator->current)->prev;
            data = listIterator->current ? listIterator->current->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_LIST_HEAD_PEEK: {
            data = listIterator->list->head ? listIterator->list->head->data : NULL;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_LIST_TAIL_PEEK: {
            data = listIterator->list->tail ? listIterator->list->tail->data : NULL;
        }
        break;
    default:
        ASSERT(0);
        return OCR_ENOTSUP;
    }
    iterator->ITERATOR_ARG_FIELD(OCR_SCHEDULER_OBJECT_LIST).el = data;
    return 0;
}

u64 listSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectList_t *schedObj = (ocrSchedulerObjectList_t*)self;
    return schedObj->list->count;
}

ocrSchedulerObject_t* listGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 listSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-LIST SCHEDULER_OBJECT FACTORY FUNCTIONS        */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectList(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryList(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryList(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ASSERT(paramFact->kind == OCR_SCHEDULER_OBJECT_AGGREGATE);
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryList_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectList_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_LIST;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectList;
    schedObjFact->destruct = &destructSchedulerObjectFactoryList;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), listSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), listSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), listSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), listSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), listSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), listSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), listSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), listGetSchedulerObjectForLocation);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_LIST */
