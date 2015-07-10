/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler-object/scheduler-object-all.h"
#include "debug.h"

const char * schedulerObject_types[] = {
#ifdef ENABLE_SCHEDULER_OBJECT_DOMAIN
    "DOMAIN",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBNODE
    "DBNODE",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    "WST",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    "DEQ",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    "LIST",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    "MAP",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    "NULL",
#endif
    NULL
};

ocrSchedulerObjectFactory_t * newSchedulerObjectFactory(schedulerObjectType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_OBJECT_DOMAIN
    case schedulerObjectDomain_id:
        return newOcrSchedulerObjectFactoryDomain(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBNODE
    case schedulerObjectDbNode_id:
        return newOcrSchedulerObjectFactoryDbNode(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_WST
    case schedulerObjectWst_id:
        return newOcrSchedulerObjectFactoryWst(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
    case schedulerObjectDeq_id:
        return newOcrSchedulerObjectFactoryDeq(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    case schedulerObjectList_id:
        return newOcrSchedulerObjectFactoryList(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    case schedulerObjectMap_id:
        return newOcrSchedulerObjectFactoryMap(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    case schedulerObjectNull_id:
        return newOcrSchedulerObjectFactoryNull(perType, (u32)type);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

