/**
* @brief Data-block implementation for OCR
*/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-allocator.h"
#include "ocr-datablock.h"
#include "ocr-db.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#include "utils/profiler/profiler.h"

#define DEBUG_TYPE API

u8 ocrDbCreate(ocrGuid_t *db, void** addr, u64 len, u16 flags,
               ocrGuid_t affinity, ocrInDbAllocator_t allocator) {

    START_PROFILE(api_DbCreate);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrDbCreate(*guid=0x%lx, len=%lu, flags=%u"
            ", aff=0x%lx, alloc=%u)\n", *db, len, (u32)flags, affinity, (u32)allocator);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *policy = NULL;
    ocrTask_t *task = NULL;
    u8 returnCode = 0;
    getCurrentEnv(&policy, NULL, &task, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_CREATE
    msg.type = PD_MSG_DB_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = *db;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_IO(properties) = (u32) flags;
    PD_MSG_FIELD_IO(size) = len;
    PD_MSG_FIELD_I(edt.guid) = task?task->guid:NULL_GUID; // Can happen when non EDT creates the DB
    PD_MSG_FIELD_I(edt.metaDataPtr) = task;
    PD_MSG_FIELD_I(affinity.guid) = affinity;
    PD_MSG_FIELD_I(affinity.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(dbType) = USER_DBTYPE;
    PD_MSG_FIELD_I(allocator) = allocator;
    returnCode = policy->fcts.processMessage(policy, &msg, true);
    if((returnCode == 0) && ((returnCode = PD_MSG_FIELD_O(returnDetail)) == 0)) {
        *db = PD_MSG_FIELD_IO(guid.guid);
        *addr = PD_MSG_FIELD_O(ptr);
    } else {
        *db = NULL_GUID;
        *addr = NULL;
    }
#undef PD_MSG
#undef PD_TYPE

    if(task && (returnCode == 0)) {
        // Here we inform the task that we created a DB
        // This is most likely ALWAYS a local message but let's leave the
        // API as it is for now. It is possible that the EDTs move at some point so
        // just to be safe
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_DYNADD
        getCurrentEnv(NULL, NULL, NULL, &msg);
        msg.type = PD_MSG_DEP_DYNADD | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(edt.guid) = task->guid;
        PD_MSG_FIELD_I(edt.metaDataPtr) = task;
        PD_MSG_FIELD_I(db.guid) = *db;
        PD_MSG_FIELD_I(db.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(properties) = 0;
        returnCode = policy->fcts.processMessage(policy, &msg, false);
        if(returnCode != 0) {
            DPRINTF(DEBUG_LVL_WARN, "EXIT ocrDbCreate -> %u; Issue registering datablock\n", returnCode);
            RETURN_PROFILE(returnCode);
        }
#undef PD_MSG
#undef PD_TYPE
    } else {
        if(!(flags & DB_PROP_IGNORE_WARN) && (returnCode == 0)) {
            DPRINTF(DEBUG_LVL_WARN, "Acquiring DB (GUID: 0x%lx) from outside an EDT ... auto-release will fail\n",
                    *db);
        }
    }
    DPRINTF(DEBUG_LVL_INFO, "EXIT ocrDbCreate -> 0; GUID: 0x%lx; ADDR: 0x%lx size: %lu\n", *db, *addr, len);
    RETURN_PROFILE(0);
}

u8 ocrDbDestroy(ocrGuid_t db) {

    START_PROFILE(api_DbDestroy);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrDbDestroy(guid=0x%lx)\n", db);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *policy = NULL;
    ocrTask_t *task = NULL;
    getCurrentEnv(&policy, NULL, &task, &msg);
    u8 returnCode = OCR_ECANCELED;
    bool dynRemoved = false;
    if(task) {
        // Here we inform the task that we are going to destroy the DB
        // We check the returnDetail of the operation to find out if the task
        // was using the datablock or not.
        // This is most likely ALWAYS a local message but let's leave the
        // API as it is for now. It is possible that the EDTs move at some point so
        // just to be safe
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_DYNREMOVE
        getCurrentEnv(NULL, NULL, NULL, &msg);
        msg.type = PD_MSG_DEP_DYNREMOVE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_I(edt.guid) = task->guid;
        PD_MSG_FIELD_I(edt.metaDataPtr) = task;
        PD_MSG_FIELD_I(db.guid) = db;
        PD_MSG_FIELD_I(db.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(properties) = 0;
        returnCode = policy->fcts.processMessage(policy, &msg, true);
        if(returnCode != 0) {
            DPRINTF(DEBUG_LVL_WARN, "Destroying DB (GUID: 0x%lx) -> %u; Issue unregistering the datablock\n", db, returnCode);
        }
        dynRemoved = (PD_MSG_FIELD_O(returnDetail)==0);
#undef PD_MSG
#undef PD_TYPE
    } else {
        DPRINTF(DEBUG_LVL_WARN, "Destroying DB (GUID: 0x%lx) from outside an EDT ... auto-release will fail\n", db);
    }
    // !task is to allow the legacy interface to destroy a datablock outside of an EDT
    if ((!task) || (returnCode == 0)) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_FREE
        msg.type = PD_MSG_DB_FREE | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(guid.guid) = db;
        PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(edt.guid) = task?task->guid:NULL_GUID;
        PD_MSG_FIELD_I(edt.metaDataPtr) = task;
        // Tell whether or not the task was using the DB. This is useful
        // to know if the DB actually needs to be released or not.
        PD_MSG_FIELD_I(properties) = dynRemoved ? 0 : DB_PROP_NO_RELEASE;
        returnCode = policy->fcts.processMessage(policy, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    } else {
        DPRINTF(DEBUG_LVL_WARN, "Destroying DB (GUID: 0x%lx) Issue destroying the datablock\n", db);
    }

    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrDbDestroy(guid=0x%lx) -> %u\n", db, returnCode);
    RETURN_PROFILE(returnCode);
}

u8 ocrDbRelease(ocrGuid_t db) {

    START_PROFILE(api_DbRelease);
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrDbRelease(guid=0x%lx)\n", db);
    PD_MSG_STACK(msg);
    ocrPolicyDomain_t *policy = NULL;
    ocrTask_t *task = NULL;
    getCurrentEnv(&policy, NULL, &task, &msg);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_RELEASE
    msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = db;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(edt.guid) = task?task->guid:NULL_GUID;
    PD_MSG_FIELD_I(edt.metaDataPtr) = task;
    PD_MSG_FIELD_I(ptr) = NULL;
    PD_MSG_FIELD_I(size) = 0;
    PD_MSG_FIELD_I(properties) = 0;
    u8 returnCode = policy->fcts.processMessage(policy, &msg, true);
    if(returnCode == 0) {
        returnCode = PD_MSG_FIELD_O(returnDetail);
    }
#undef PD_MSG
#undef PD_TYPE

    if(task && (returnCode == 0)) {
        // Here we inform the task that we released a DB
        // This is most likely ALWAYS a local message but let's leave the
        // API as it is for now. It is possible that the EDTs move at some point so
        // just to be safe
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_DYNREMOVE
        getCurrentEnv(NULL, NULL, NULL, &msg);
        msg.type = PD_MSG_DEP_DYNREMOVE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_I(edt.guid) = task->guid;
        PD_MSG_FIELD_I(edt.metaDataPtr) = task;
        PD_MSG_FIELD_I(db.guid) = db;
        PD_MSG_FIELD_I(db.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(properties) = 0;
        returnCode = policy->fcts.processMessage(policy, &msg, true);
        if (returnCode != 0) {
            DPRINTF(DEBUG_LVL_WARN, "Releasing DB  -> %u; Issue unregistering DB datablock\n", returnCode);
        }
#undef PD_MSG
#undef PD_TYPE
    } else {
        if (returnCode == 0) {
            DPRINTF(DEBUG_LVL_WARN, "Releasing DB (GUID: 0x%lx) from outside an EDT ... auto-release will fail\n", db);
        }
    }

    DPRINTF_COND_LVL(returnCode, DEBUG_LVL_WARN, DEBUG_LVL_INFO,
                     "EXIT ocrDbRelease(guid=0x%lx) -> %u\n", db, returnCode);
    RETURN_PROFILE(returnCode);
}

u8 ocrDbMalloc(ocrGuid_t guid, u64 size, void** addr) {
    return OCR_EINVAL; /* not yet implemented */
}

u8 ocrDbMallocOffset(ocrGuid_t guid, u64 size, u64* offset) {
    return OCR_EINVAL; /* not yet implemented */
}

u8 ocrDbCopy(ocrGuid_t destination, u64 destinationOffset, ocrGuid_t source,
             u64 sourceOffset, u64 size, u64 copyType, ocrGuid_t *completionEvt) {
    return OCR_EINVAL; /* not yet implemented */
}

u8 ocrDbFree(ocrGuid_t guid, void* addr) {
    return OCR_EINVAL; /* not yet implemented */
}

u8 ocrDbFreeOffset(ocrGuid_t guid, u64 offset) {
    return OCR_EINVAL; /* not yet implemented */
}
