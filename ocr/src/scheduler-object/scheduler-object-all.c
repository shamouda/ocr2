/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "scheduler-object/scheduler-object-all.h"
#include "debug.h"

const char * schedulerObject_types[] = {
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    "NULL",
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
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
    "PDSPACE",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
    "DBSPACE",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
    "DBTIME",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
    "PR_WSH",
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    "BIN_HEAP",
#endif
    NULL
};

ocrSchedulerObjectFactory_t * newSchedulerObjectFactory(schedulerObjectType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_SCHEDULER_OBJECT_NULL
    case schedulerObjectNull_id:
        return newOcrSchedulerObjectFactoryNull(perType, (u32)type);
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
#ifdef ENABLE_SCHEDULER_OBJECT_PDSPACE
    case schedulerObjectPdspace_id:
        return newOcrSchedulerObjectFactoryPdspace(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBSPACE
    case schedulerObjectDbspace_id:
        return newOcrSchedulerObjectFactoryDbspace(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_DBTIME
    case schedulerObjectDbtime_id:
        return newOcrSchedulerObjectFactoryDbtime(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_PR_WSH
    case schedulerObjectPrWsh_id:
        return newOcrSchedulerObjectFactoryPrWsh(perType, (u32)type);
#endif
#ifdef ENABLE_SCHEDULER_OBJECT_BIN_HEAP
    case schedulerObjectBinHeap_id:
        return newOcrSchedulerObjectFactoryBinHeap(perType, (u32)type);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

