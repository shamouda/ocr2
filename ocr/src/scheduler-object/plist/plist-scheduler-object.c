/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_PLIST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/plist/plist-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-PLIST SCHEDULER_OBJECT FUNCTIONS                */
/******************************************************/

static void plistSchedulerObjectStart(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrListType plistType, u32 elSize, u32 arrayChunkSize) {
    self->loc = PD->myLocation;
    self->mapping = OCR_SCHEDULER_OBJECT_MAPPING_PINNED;
    ocrSchedulerObjectPlist_t* plistSchedObj = (ocrSchedulerObjectPlist_t*)self;
    plistSchedObj->plist = newArrayList(elSize, arrayChunkSize, plistType);
}

static void plistSchedulerObjectInitialize(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    self->guid.guid = NULL_GUID;
    self->guid.metaDataPtr = self;
    self->kind = OCR_SCHEDULER_OBJECT_PLIST;
    self->fctId = fact->factoryId;
    self->loc = INVALID_LOCATION;
    self->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
    ocrSchedulerObjectPlist_t* plistSchedObj = (ocrSchedulerObjectPlist_t*)self;
    plistSchedObj->plist = NULL;
    plistSchedObj->lock = 0;
}

ocrSchedulerObject_t* newSchedulerObjectPlist(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)perInstance;
    ASSERT(paramSchedObj->config);
    ASSERT(!paramSchedObj->guidRequired);
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectPlist_t), PERSISTENT_CHUNK);
    plistSchedulerObjectInitialize(factory, schedObj);
    schedObj->kind |= OCR_SCHEDULER_OBJECT_ALLOC_CONFIG;
    return schedObj;
}

ocrSchedulerObject_t* plistSchedulerObjectCreate(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)perInstance;
    ASSERT(!paramSchedObj->config);
    ASSERT(!paramSchedObj->guidRequired);
    ocrPolicyDomain_t *pd = factory->pd;
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectPlist_t));
    plistSchedulerObjectInitialize(factory, schedObj);
    paramListSchedulerObjectPlist_t *paramsPlist = (paramListSchedulerObjectPlist_t*)perInstance;
    plistSchedulerObjectStart(schedObj, pd, paramsPlist->type, paramsPlist->elSize, paramsPlist->arrayChunkSize);
    schedObj->kind |= OCR_SCHEDULER_OBJECT_ALLOC_PD;
    return schedObj;
}

u8 plistSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    if (IS_SCHEDULER_OBJECT_CONFIG_ALLOCATED(self->kind)) {
        runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
    } else {
        ASSERT(IS_SCHEDULER_OBJECT_PD_ALLOCATED(self->kind));
        ocrPolicyDomain_t *pd = fact->pd;
        ocrSchedulerObjectPlist_t* plistSchedObj = (ocrSchedulerObjectPlist_t*)self;
        destructArrayList(plistSchedObj->plist);
        pd->fcts.pdFree(pd, self);
    }
    return 0;
}

u8 plistSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    u64 priority = 0;
    slistNode_t *node = NULL;
    ocrSchedulerObjectPlist_t *plistObj = (ocrSchedulerObjectPlist_t*)self;
    switch(properties & SCHEDULER_OBJECT_INSERT_POSITION) {
    case SCHEDULER_OBJECT_INSERT_POSITION_HEAD:
        {
            switch(properties & SCHEDULER_OBJECT_INSERT_KIND) {
            case SCHEDULER_OBJECT_INSERT_BEFORE:
                {
                    pushFrontArrayList(plistObj->plist, element);
                }
                break;
            case SCHEDULER_OBJECT_INSERT_AFTER:
                {
                    node = newArrayListNodeAfter(plistObj->plist, plistObj->plist->head);
                }
                break;
            case SCHEDULER_OBJECT_INSERT_INPLACE:
                {
                    node = plistObj->plist->head;
                }
                break;
            default:
                ASSERT(0);
                break;
            }
        }
        break;
    case SCHEDULER_OBJECT_INSERT_POSITION_TAIL:
        {
            switch(properties & SCHEDULER_OBJECT_INSERT_KIND) {
            case SCHEDULER_OBJECT_INSERT_BEFORE:
                {
                    node = newArrayListNodeBefore(plistObj->plist, plistObj->plist->tail);
                }
                break;
            case SCHEDULER_OBJECT_INSERT_AFTER:
                {
                    pushBackArrayList(plistObj->plist, element);
                }
                break;
            case SCHEDULER_OBJECT_INSERT_INPLACE:
                {
                    node = plistObj->plist->tail;
                }
                break;
            default:
                ASSERT(0);
                break;
            }
        }
        break;
    case SCHEDULER_OBJECT_INSERT_POSITION_ITERATOR:
        {
            ASSERT(iterator && iterator->data);
            priority = (u64)(iterator->data);
            node = newArrayListNodePriority(plistObj->plist, priority);
        }
        break;
    default:
        ASSERT(0);
        break;
    }
    if (node) {
        if (plistObj->plist->elSize) {
            hal_memCopy(node->data, element, plistObj->plist->elSize, 0);
        } else {
            if (IS_SCHEDULER_OBJECT_TYPE_SINGLETON(element->kind)) {
                //PRINTF("INSERTED GUID %lx PRIORITY: %lu\n", (u64)element->guid.guid, priority);
                node->data = (void*)element->guid.guid;
            } else {
                node->data = element;
            }
        }
    }
    return 0;
}

u8 plistSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    slistNode_t *node = NULL;
    ocrSchedulerObjectPlist_t *plistObj = (ocrSchedulerObjectPlist_t*)self;
    switch(properties) {
    case SCHEDULER_OBJECT_REMOVE_HEAD:
        {
            node = plistObj->plist->head;
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_TAIL:
        {
            node = plistObj->plist->tail;
        }
        break;
    case SCHEDULER_OBJECT_REMOVE_ITERATOR:
        {
            ocrSchedulerObjectPlistIterator_t *lit = (ocrSchedulerObjectPlistIterator_t*)iterator;
            node = lit->current;
        }
        break;
    default:
        ASSERT(0);
        break;
    }
    if (node) {
        if (plistObj->plist->elSize) {
            if (IS_SCHEDULER_OBJECT_TYPE_SINGLETON(dst->kind)) {
                ASSERT(plistObj->plist->elSize == sizeof(ocrGuid_t));
                dst->guid.guid = *((u64*)(node->data));
            } else {
                ASSERT(dst->guid.metaDataPtr);
                hal_memCopy(dst->guid.metaDataPtr, node->data, plistObj->plist->elSize, 0);
            }
        } else {
            if (IS_SCHEDULER_OBJECT_TYPE_SINGLETON(kind)) {
                //PRINTF("REMOVED GUID %lx PRIORITY: %lu\n", (u64)node->data, node->key);
                dst->guid.guid = (ocrGuid_t)node->data;
                dst->guid.metaDataPtr = NULL;
            } else {
                dst->guid.metaDataPtr = node->data;
            }
        }
        freeArrayListNode(plistObj->plist, node);
    }
    return 0;
}

u64 plistSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectPlist_t *schedObj = (ocrSchedulerObjectPlist_t*)self;
    return schedObj->plist->count;
}

ocrSchedulerObjectIterator_t* plistSchedulerObjectCreateIterator(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObjectIterator_t* iterator = (ocrSchedulerObjectIterator_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectPlistIterator_t));
    iterator->schedObj = self;
    iterator->data = NULL;
    ocrSchedulerObjectPlistIterator_t *lit = (ocrSchedulerObjectPlistIterator_t*)iterator;
    ocrSchedulerObjectPlist_t *plistObj = (ocrSchedulerObjectPlist_t*)self;
    lit->current = plistObj->plist->head;
    return iterator;
}

u8 plistSchedulerObjectDestroyIterator(ocrSchedulerObjectFactory_t * fact, ocrSchedulerObjectIterator_t *iterator) {
    ocrPolicyDomain_t *pd = fact->pd;
    pd->fcts.pdFree(pd, iterator);
    return 0;
}

u8 plistSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ocrSchedulerObjectPlist_t *plistObj = (ocrSchedulerObjectPlist_t*)iterator->schedObj;
    ocrSchedulerObjectPlistIterator_t *lit = (ocrSchedulerObjectPlistIterator_t*)iterator;
    iterator->data = NULL;
    switch(properties) {
    case SCHEDULER_OBJECT_ITERATE_CURRENT:
        {
            if (lit->current) iterator->data = lit->current->data;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_HEAD:
        {
            lit->current = plistObj->plist->head;
            if (lit->current) iterator->data = lit->current->data;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_TAIL:
        {
            lit->current = plistObj->plist->tail;
            if (lit->current) iterator->data = lit->current->data;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_NEXT:
        {
            if (lit->current) lit->current = lit->current->next;
            if (lit->current) iterator->data = lit->current->data;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_PREV:
        {
            if (lit->current) {
                if (plistObj->plist->type == OCR_LIST_TYPE_DOUBLE) {
                    lit->current = ((dlistNode_t*)(lit->current))->prev;
                } else {
                    slistNode_t *prev = plistObj->plist->head;
                    while (prev && prev->next != lit->current) prev = prev->next;
                    lit->current = prev;
                }
            }
            if (lit->current) iterator->data = lit->current->data;
        }
        break;
    case SCHEDULER_OBJECT_ITERATE_SEARCH_KEY:
        {
            ASSERT(0); //TODO
        }
        break;
    default:
        ASSERT(0);
        break;
    }
    return 0;
}

ocrSchedulerObject_t* plistGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 plistSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

ocrSchedulerObjectActionSet_t* plistSchedulerObjectNewActionSet(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 count) {
    ASSERT(0);
    return NULL;
}

u8 plistSchedulerObjectDestroyActionSet(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObjectActionSet_t *actionSet) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 plistSchedulerObjectSwitchRunlevel(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                    phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 plistSchedulerObjectOcrPolicyMsgGetMsgSize(ocrSchedulerObjectFactory_t *fact, ocrPolicyMsg_t *msg, u64 *marshalledSize, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 plistSchedulerObjectOcrPolicyMsgMarshallMsg(ocrSchedulerObjectFactory_t *fact, ocrPolicyMsg_t *msg, u8 *buffer, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 plistSchedulerObjectOcrPolicyMsgUnMarshallMsg(ocrSchedulerObjectFactory_t *fact, ocrPolicyMsg_t *msg, u8 *localMainPtr, u8 *localAddlPtr, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-PLIST SCHEDULER_OBJECT FACTORY FUNCTIONS        */
/******************************************************/

void destructSchedulerObjectFactoryPlist(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryPlist(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryPlist_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectPlist_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_PLIST;
    schedObjFact->pd = NULL;

    schedObjFact->destruct = &destructSchedulerObjectFactoryPlist;
    schedObjFact->instantiate = &newSchedulerObjectPlist;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), plistSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), plistSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), plistSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), plistSchedulerObjectRemove);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), plistSchedulerObjectCount);
    schedObjFact->fcts.createIterator = FUNC_ADDR(ocrSchedulerObjectIterator_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), plistSchedulerObjectCreateIterator);
    schedObjFact->fcts.destroyIterator = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObjectIterator_t*), plistSchedulerObjectDestroyIterator);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObjectIterator_t*, u32), plistSchedulerObjectIterate);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), plistSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), plistGetSchedulerObjectForLocation);
    schedObjFact->fcts.createActionSet = FUNC_ADDR(ocrSchedulerObjectActionSet_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), plistSchedulerObjectNewActionSet);
    schedObjFact->fcts.destroyActionSet = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObjectActionSet_t*), plistSchedulerObjectDestroyActionSet);
    schedObjFact->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), plistSchedulerObjectSwitchRunlevel);
    schedObjFact->fcts.ocrPolicyMsgGetMsgSize = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrPolicyMsg_t*, u64*, u32), plistSchedulerObjectOcrPolicyMsgGetMsgSize);
    schedObjFact->fcts.ocrPolicyMsgMarshallMsg = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrPolicyMsg_t*, u8*, u32), plistSchedulerObjectOcrPolicyMsgMarshallMsg);
    schedObjFact->fcts.ocrPolicyMsgUnMarshallMsg = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrPolicyMsg_t*, u8*, u8*, u32), plistSchedulerObjectOcrPolicyMsgUnMarshallMsg);
    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_PLIST */
