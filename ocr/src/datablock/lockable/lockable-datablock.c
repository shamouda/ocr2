/**
 * @brief Simple implementation of a malloc wrapper
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_DATABLOCK_LOCKABLE

#include "ocr-hal.h"
#include "datablock/lockable/lockable-datablock.h"
#include "debug.h"
#include "ocr-comp-platform.h"
#include "ocr-datablock.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#define DEBUG_TYPE DATABLOCK

#define DB_LOCKED_NONE 0
#define DB_LOCKED_EW 1
#define DB_LOCKED_ITW 2

/***********************************************************/
/* OCR-Lockable Datablock Hint Properties                  */
/* (Add implementation specific supported properties here) */
/***********************************************************/

u64 ocrHintPropDbLockable[] = {
#ifdef ENABLE_HINTS
    OCR_HINT_DB_AFFINITY
#endif
};

//Make sure OCR_HINT_COUNT_DB_LOCKABLE in regular-datablock.h is equal to the length of array ocrHintPropDbLockable
ocrStaticAssert((sizeof(ocrHintPropDbLockable)/sizeof(u64)) == OCR_HINT_COUNT_DB_LOCKABLE);
ocrStaticAssert(OCR_HINT_COUNT_DB_LOCKABLE < OCR_RUNTIME_HINT_PROP_BITS);

/******************************************************/
/* OCR-Lockable Datablock                             */
/******************************************************/

// Data-structure to store EDT waiting to be be granted access to the DB
typedef struct _dbWaiter_t {
    ocrGuid_t guid;
    u32 slot;
    u32 properties; // properties specified with the acquire request
    bool isInternal;
    struct _dbWaiter_t * next;
} dbWaiter_t;

// Forward declaration
u8 lockableDestruct(ocrDataBlock_t *self);

// simple helper function to resolve the location of a guid
static ocrLocation_t fatGuidToLocation(ocrPolicyDomain_t * pd, ocrFatGuid_t fatGuid) {
    // at startup this code may be run outside of an EDT
    if (ocrGuidIsNull(fatGuid.guid)) {
        return pd->myLocation;
    } else {
        ocrLocation_t edtLoc = INVALID_LOCATION;
        u8 res __attribute__((unused)) = guidLocation(pd, fatGuid, &edtLoc);
        // Check that the GUID is valid
        ASSERT(!res);
        return edtLoc;
    }
}

static ocrLocation_t guidToLocation(ocrPolicyDomain_t * pd, ocrGuid_t edtGuid) {
    // at startup this code may be run outside of an EDT
    if (ocrGuidIsNull(edtGuid)) {
        return pd->myLocation;
    } else {
        ocrFatGuid_t fatGuid;
        fatGuid.guid = edtGuid;
        fatGuid.metaDataPtr = NULL;
        ocrLocation_t edtLoc = INVALID_LOCATION;
        u8 res __attribute__((unused)) = guidLocation(pd, fatGuid, &edtLoc);
        // Check that the GUID is valid
        ASSERT(!res);
        return edtLoc;
    }
}

//Warning: Calling context must own rself->lock
static dbWaiter_t * popEwWaiter(ocrDataBlock_t * self) {
    ocrDataBlockLockable_t * rself = (ocrDataBlockLockable_t*) self;
    ASSERT(rself->ewWaiterList != NULL);
    dbWaiter_t * waiter = rself->ewWaiterList;
    rself->ewWaiterList = waiter->next;
    return waiter;
}

//Warning: Calling context must own rself->lock
static dbWaiter_t * popItwWaiter(ocrDataBlock_t * self) {
    ocrDataBlockLockable_t * rself = (ocrDataBlockLockable_t*) self;
    ASSERT(rself->itwWaiterList != NULL);
    dbWaiter_t * waiter = rself->itwWaiterList;
    rself->itwWaiterList = waiter->next;
    return waiter;
}

//Warning: Calling context must own rself->lock
static dbWaiter_t * popRoWaiter(ocrDataBlock_t * self) {
    ocrDataBlockLockable_t * rself = (ocrDataBlockLockable_t*) self;
    ASSERT(rself->roWaiterList != NULL);
    dbWaiter_t * waiter = rself->roWaiterList;
    rself->roWaiterList = waiter->next;
    return waiter;
}

static bool lockButSelf(ocrDataBlockLockable_t *rself) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    bool unlock = true;
    if (rself->lock) {
        if (worker == rself->worker) {
            // fall-through
            unlock = false;
        } else {
            hal_lock32(&rself->lock);
        }
    } else {
        hal_lock32(&rself->lock);
        rself->worker = worker;
    }
    return unlock;
}

//Warning: This call must be protected with the self->lock in the calling context
//
//If the datablock is not available for immediate acquisition, the implementation
//asserts on edtSlot not being equal to EDT_SLOT_NONE. The current runtime implementation
//only supports asynchronous acquisition of datablocks from EDT's dependences (i.e. the call
//site of acquire is written in a way that supports an asynchronous callback).
static u8 lockableAcquireInternal(ocrDataBlock_t *self, void** ptr, ocrFatGuid_t edt, u32 edtSlot,
                  ocrDbAccessMode_t mode, bool isInternal, u32 properties) {
    ocrDataBlockLockable_t * rself = (ocrDataBlockLockable_t*) self;

    if(rself->attributes.freeRequested && (rself->attributes.numUsers == 0)) {
        // Most likely stemming from an error in the user-code
        // There's a race between the datablock being freed and having no
        // users with someone else trying to acquire the DB
        ASSERT(false && "OCR_EACCES");
        return OCR_EACCES;
    }

    // Allows the runtime to directly access the data pointer.
    if(properties & DB_PROP_RT_OBLIVIOUS) {
        *ptr = self->ptr;
        return 0;
    }

    // mode == DB_MODE_RO just fall through

    if (mode == DB_MODE_CONST) {
        if (rself->attributes.modeLock) {
            ASSERT(edtSlot != EDT_SLOT_NONE);
            ocrPolicyDomain_t * pd = NULL;
            getCurrentEnv(&pd, NULL, NULL, NULL);
            dbWaiter_t * waiterEntry = (dbWaiter_t *) pd->fcts.pdMalloc(pd, sizeof(dbWaiter_t));
            waiterEntry->guid = edt.guid;
            waiterEntry->slot = edtSlot;
            waiterEntry->isInternal = isInternal;
            waiterEntry->properties = properties;
            waiterEntry->next = rself->roWaiterList;
            rself->roWaiterList = waiterEntry;
            *ptr = NULL; // not contractual, but should ease debug if misread
            return OCR_EBUSY;
        }
    }

    if (mode == DB_MODE_EW) {
        if ((rself->attributes.modeLock) || (rself->attributes.numUsers != 0)) {
            ASSERT(edtSlot != EDT_SLOT_NONE);
            // The DB is already in use
            ocrPolicyDomain_t * pd = NULL;
            getCurrentEnv(&pd, NULL, NULL, NULL);
            dbWaiter_t * waiterEntry = (dbWaiter_t *) pd->fcts.pdMalloc(pd, sizeof(dbWaiter_t));
            waiterEntry->guid = edt.guid;
            waiterEntry->slot = edtSlot;
            waiterEntry->isInternal = isInternal;
            waiterEntry->properties = properties;
            waiterEntry->next = rself->ewWaiterList;
            rself->ewWaiterList = waiterEntry;
            *ptr = NULL; // not contractual, but should ease debug if misread
            return OCR_EBUSY;
        } else {
            rself->attributes.modeLock = DB_LOCKED_EW;
        }
    }

    if (mode == DB_MODE_RW) {
        bool enque = false;
        if (rself->attributes.modeLock == DB_LOCKED_ITW) {
            ocrPolicyDomain_t * pd = NULL;
            getCurrentEnv(&pd, NULL, NULL, NULL);
            // check of DB already in ITW use by another location
            enque = (fatGuidToLocation(pd, edt) != rself->itwLocation);
        } else {
            enque = (rself->attributes.numUsers != 0) || (rself->attributes.modeLock == DB_LOCKED_EW);
        }
        if (enque) {
            ASSERT(edtSlot != EDT_SLOT_NONE);
            // The DB is already in use, enque
            ocrPolicyDomain_t * pd = NULL;
            getCurrentEnv(&pd, NULL, NULL, NULL);
            dbWaiter_t * waiterEntry = (dbWaiter_t *) pd->fcts.pdMalloc(pd, sizeof(dbWaiter_t));
            waiterEntry->guid = edt.guid;
            waiterEntry->slot = edtSlot;
            waiterEntry->isInternal = isInternal;
            waiterEntry->properties = properties;
            waiterEntry->next = rself->itwWaiterList;
            rself->itwWaiterList = waiterEntry;
            *ptr = NULL; // not contractual, but should ease debug if misread
            return OCR_EBUSY;
        } else {
            // Ruled out all enque scenario, we can acquire.
            // Just check if we need to grab lock too
            if (rself->attributes.numUsers == 0) {
                ocrPolicyDomain_t * pd = NULL;
                getCurrentEnv(&pd, NULL, NULL, NULL);
                rself->attributes.modeLock = DB_LOCKED_ITW;
                rself->itwLocation = fatGuidToLocation(pd, edt);
            }
        }
    }

    rself->attributes.numUsers += 1;
    DPRINTF(DEBUG_LVL_VERB, "Acquiring DB @ 0x%"PRIx64" (GUID: "GUIDF") from EDT (GUID: "GUIDF") (runtime acquire: %"PRId32") (mode: %"PRId32") (numUsers: %"PRId32") (modeLock: %"PRId32")\n",
            (u64)self->ptr, GUIDA(rself->base.guid), GUIDA(edt.guid), (u32)isInternal, (int) mode,
            rself->attributes.numUsers, rself->attributes.modeLock);

#ifdef OCR_ENABLE_STATISTICS
    {
        statsDB_ACQ(pd, edt.guid, (ocrTask_t*)edt.metaDataPtr, self->guid, self);
    }
#endif /* OCR_ENABLE_STATISTICS */
    *ptr = self->ptr;
    return 0;
}


/**
 * @brief Setup a callback response message and acquire on behalf of the waiter
 **/
static void processAcquireCallback(ocrDataBlock_t *self, dbWaiter_t * waiter, ocrDbAccessMode_t waiterMode, u32 properties, ocrPolicyMsg_t * msg) {
    ASSERT(waiter->slot != EDT_SLOT_NONE);
    getCurrentEnv(NULL, NULL, NULL, msg);
    //BUG #273: The In/Out nature of certain parameters is exposed here
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
    msg->type = PD_MSG_DB_ACQUIRE | PD_MSG_RESPONSE;
    PD_MSG_FIELD_IO(guid.guid) = self->guid;
    PD_MSG_FIELD_IO(guid.metaDataPtr) = self;
    PD_MSG_FIELD_IO(edt.guid) = waiter->guid;
    PD_MSG_FIELD_IO(edt.metaDataPtr) = NULL;
    PD_MSG_FIELD_IO(edtSlot) = waiter->slot;
    // In this implementation properties encodes the MODE + isInternal +
    // any additional flags set by the PD (such as the FETCH flag)
    PD_MSG_FIELD_IO(properties) = properties;
    // A response msg is being built, must set all the OUT fields
    PD_MSG_FIELD_O(size) = self->size;
    PD_MSG_FIELD_O(returnDetail) = 0;
    //NOTE: we still have the lock, calling the internal version
    u8 res __attribute__((unused)) = lockableAcquireInternal(self, &PD_MSG_FIELD_O(ptr), PD_MSG_FIELD_IO(edt),
                                  PD_MSG_FIELD_IO(edtSlot), waiterMode, waiter->isInternal,
                                  PD_MSG_FIELD_IO(properties));
    // Not much we would be able to recover here
    ASSERT(!res);
#undef PD_MSG
#undef PD_TYPE
}

u8 lockableAcquire(ocrDataBlock_t *self, void** ptr, ocrFatGuid_t edt, u32 edtSlot,
                  ocrDbAccessMode_t mode, bool isInternal, u32 properties) {
    ocrDataBlockLockable_t *rself = (ocrDataBlockLockable_t*)self;
    bool unlock = lockButSelf(rself);
    u8 res = lockableAcquireInternal(self, ptr, edt, edtSlot, mode, isInternal, properties);
    if (unlock) {
        rself->worker = NULL;
        hal_unlock32(&rself->lock);
    }
    return res;
}

u8 lockableRelease(ocrDataBlock_t *self, ocrFatGuid_t edt, bool isInternal) {
    ocrDataBlockLockable_t *rself = (ocrDataBlockLockable_t*)self;
    dbWaiter_t * waiter = NULL;
    DPRINTF(DEBUG_LVL_VERB, "Releasing DB @ 0x%"PRIx64" (GUID "GUIDF") from EDT "GUIDF" (runtime release: %"PRId32")\n",
            (u64)self->ptr, GUIDA(rself->base.guid), GUIDA(edt.guid), (u32)isInternal);
    // Start critical section
    hal_lock32(&(rself->lock));
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    rself->worker = worker;

    // The registered EDT can be different if a DB has been released by the user
    // and the runtime tries to release it afterwards. It could be that in
    // between the two releases, the slot is used for another EDT's DB.
    rself->attributes.numUsers -= 1;

    // catch errors when release is called one too many time
    ASSERT(rself->attributes.numUsers != (u32)-1);

    //IMPL: this is probably not very fair
    if (rself->attributes.numUsers == 0) {
        // last user of the DB, check what the transition looks like:
        // W -> W (pop the next EW waiter)
        // RO -> EW (pop the next EW waiter)
        // EW -> RO (pop all RO waiter)
        // RO -> RO (nothing to do)
        if (rself->attributes.modeLock) {
            // Last ITW writer. Most likely there are either more ITW on their way
            // or we're done writing and there are RO piling up or on their way.
            rself->attributes.modeLock = DB_LOCKED_NONE; // release and see what we got
            rself->itwLocation = INVALID_LOCATION;
            if (rself->roWaiterList != NULL) {
                waiter = popRoWaiter(self);
            }
        } else {
            // We're in RO, by all means there should be no RO waiter
            ASSERT(rself->roWaiterList == NULL);
            // Try to favor writers
        }

        if (waiter == NULL) {
            if (rself->itwWaiterList != NULL) {
                waiter = popItwWaiter(self);
                rself->attributes.modeLock = DB_LOCKED_ITW;
            } else if (rself->ewWaiterList != NULL) {
                waiter = popEwWaiter(self);
                rself->attributes.modeLock = DB_LOCKED_EW;
            }
        }

        if (rself->attributes.modeLock == DB_LOCKED_ITW) {
            // NOTE: by design there should be a waiter otherwise we would have exit the modeLock
            ASSERT(waiter != NULL);
            // Switching to ITW mode, now we can release all waiters that belong
            // to the same location the elected waiter is.
            ocrPolicyDomain_t * pd = NULL;
            PD_MSG_STACK(msg);
            getCurrentEnv(&pd, NULL, NULL, NULL);
            // Setup: Have ITW lock + its right location for acquire
            ocrLocation_t itwLocation = guidToLocation(pd, waiter->guid);
            rself->itwLocation = itwLocation;
            dbWaiter_t * prev = waiter;
            do {
                dbWaiter_t * next = waiter->next;
                if (itwLocation == guidToLocation(pd, waiter->guid)) {
                    processAcquireCallback(self, waiter, DB_MODE_RW, waiter->properties, &msg);
                    if (prev == waiter) { // removing head
                        prev = next;
                        rself->itwWaiterList = prev;
                    } else {
                        prev->next = next;
                    }
                    pd->fcts.pdFree(pd, waiter);
                    waiter = next;
                    //PERF: Would be nice to do that outside the lock but it incurs allocating
                    // an array of messages and traversing the list of waiters again
                    RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
                } else {
                    prev = waiter;
                    waiter = next;
                }
            } while (waiter != NULL);
            #ifdef OCR_ENABLE_STATISTICS
                {
                    statsDB_REL(getCurrentPD(), edt.guid, (ocrTask_t*)edt.metaDataPtr, self->guid, self);
                }
            #endif /* OCR_ENABLE_STATISTICS */
            rself->worker = NULL;
            hal_unlock32(&(rself->lock));
            return 0;
        } else if (rself->attributes.modeLock == DB_LOCKED_EW) {
            // EW: by design there should be a waiter otherwise we would have exit the modeLock
            ASSERT(waiter != NULL);
            ocrPolicyDomain_t * pd = NULL;
            PD_MSG_STACK(msg);
            getCurrentEnv(&pd, NULL, NULL, &msg);
            rself->attributes.modeLock = DB_LOCKED_NONE; // technicality so that acquire sees the DB is not acquired
            // Acquire the DB on behalf of the next waiter (i.e. numUser++)
            processAcquireCallback(self, waiter, DB_MODE_EW, waiter->properties, &msg);
            // Will process asynchronous callback message outside of the critical section
            #ifdef OCR_ENABLE_STATISTICS
                {
                    statsDB_REL(getCurrentPD(), edt.guid, (ocrTask_t*)edt.metaDataPtr, self->guid, self);
                }
            #endif /* OCR_ENABLE_STATISTICS */
            rself->worker = NULL;
            hal_unlock32(&(rself->lock));
            pd->fcts.pdFree(pd, waiter);
            RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
            return 0;
        } else { // RO
            //NOTE: if waiter is NULL it means there was nobody queued up for any mode
            if (waiter != NULL) {
                // transition EW -> RO, release all RO waiters
                ocrPolicyDomain_t * pd = NULL;
                PD_MSG_STACK(msg);
                getCurrentEnv(&pd, NULL, NULL, NULL);
                rself->roWaiterList = NULL;
                do {
                    processAcquireCallback(self, waiter, DB_MODE_CONST, waiter->properties, &msg);
                    dbWaiter_t * next = waiter->next;
                    pd->fcts.pdFree(pd, waiter);
                    //PERF: Would be nice to do that outside the lock but it incurs allocating
                    // an array of messages and traversing the list of waiters again
                    RESULT_ASSERT(pd->fcts.processMessage(pd, &msg, true), ==, 0);
                    waiter = next;
                } while (waiter != NULL);
                ASSERT(rself->roWaiterList == NULL);
                #ifdef OCR_ENABLE_STATISTICS
                    {
                        statsDB_REL(getCurrentPD(), edt.guid, (ocrTask_t*)edt.metaDataPtr, self->guid, self);
                    }
                #endif /* OCR_ENABLE_STATISTICS */
                rself->worker = NULL;
                hal_unlock32(&(rself->lock));
                return 0;
            }
        }
    }
    DPRINTF(DEBUG_LVL_VVERB, "DB (GUID: "GUIDF") attributes: numUsers %"PRId32" (including %"PRId32" runtime users); freeRequested %"PRId32"\n",
            GUIDA(self->guid), rself->attributes.numUsers, rself->attributes.internalUsers, rself->attributes.freeRequested);

#ifdef OCR_ENABLE_STATISTICS
    {
        statsDB_REL(getCurrentPD(), edt.guid, (ocrTask_t*)edt.metaDataPtr, self->guid, self);
    }
#endif /* OCR_ENABLE_STATISTICS */
    // Check if we need to free the block
    if(rself->attributes.numUsers == 0 &&
        rself->attributes.internalUsers == 0 &&
        rself->attributes.freeRequested == 1) {
        rself->worker = NULL;
        hal_unlock32(&(rself->lock));
        return lockableDestruct(self);
    }
    rself->worker = NULL;
    hal_unlock32(&(rself->lock));
    return 0;
}

u8 lockableDestruct(ocrDataBlock_t *self) {
    DPRINTF(DEBUG_LVL_VERB, "Freeing DB (GUID: "GUIDF")\n", GUIDA(self->guid));
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);

    if (self->flags & DB_PROP_RT_PROXY) {
        pd->fcts.pdFree(pd, self);
        return 0;
    }

#ifdef OCR_ASSERT
    ocrDataBlockLockable_t *rself = (ocrDataBlockLockable_t*)self;
    // Any of these wrong would indicate a race between free and DB's consumers
    ASSERT(rself->attributes.numUsers == 0);
    ASSERT(rself->attributes.internalUsers == 0);
    ASSERT(rself->attributes.freeRequested == 1);
    ASSERT(rself->roWaiterList == NULL);
    ASSERT(rself->ewWaiterList == NULL);
    ASSERT(rself->itwWaiterList == NULL);
    ASSERT(rself->lock == 0);
#endif

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_MEM_UNALLOC
    msg.type = PD_MSG_MEM_UNALLOC | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(allocatingPD.guid) = self->allocatingPD;
    PD_MSG_FIELD_I(allocatingPD.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(allocator.guid) = self->allocator;
    PD_MSG_FIELD_I(allocator.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(ptr) = self->ptr;
    PD_MSG_FIELD_I(type) = DB_MEMTYPE;
    PD_MSG_FIELD_I(properties) = 0;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));


#ifdef OCR_ENABLE_STATISTICS
    // This needs to be done before GUID is freed.
    {
        ocrTask_t *task = NULL;
        getCurrentEnv(NULL, NULL, &task, NULL);
        statsDB_DESTROY(pd, task->guid, task, self->allocator, NULL, self->guid, self);
    }
#endif /* OCR_ENABLE_STATISTICS */
#undef PD_TYPE
#define PD_TYPE PD_MSG_GUID_DESTROY
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    // These next two statements may be not required. Just to be safe
    PD_MSG_FIELD_I(guid.guid) = self->guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = self;
    PD_MSG_FIELD_I(properties) = 1; // Free metadata
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 lockableFree(ocrDataBlock_t *self, ocrFatGuid_t edt, u32 properties) {
    bool isInternal = ((properties & DB_PROP_RT_ACQUIRE) != 0);
    bool reqRelease = ((properties & DB_PROP_NO_RELEASE) == 0);
    ocrDataBlockLockable_t *rself = (ocrDataBlockLockable_t*)self;
    DPRINTF(DEBUG_LVL_VERB, "Requesting a free for DB @ 0x%"PRIx64" (GUID: "GUIDF"); props: 0x%"PRIx32"\n",
            (u64)self->ptr, GUIDA(rself->base.guid), properties);

    hal_lock32(&(rself->lock));
    if(rself->attributes.freeRequested) {
        hal_unlock32(&(rself->lock));
        return OCR_EPERM;
    }

    rself->attributes.freeRequested = 1;
    if(rself->attributes.numUsers == 0 && rself->attributes.internalUsers == 0) {
        hal_unlock32(&(rself->lock));
        return lockableDestruct(self);
    }
    else {
        hal_unlock32(&(rself->lock));
        // The datablock may not have been acquired by the current EDT hence
        // we do not need to account for a release.
        if (reqRelease) {
            DPRINTF(DEBUG_LVL_VVERB, "Free triggering release for DB @ 0x%"PRIx64" (GUID: "GUIDF")\n",
                    (u64)self->ptr, GUIDA(rself->base.guid));
            lockableRelease(self, edt, isInternal);
        }
    }
    return 0;
}

u8 lockableRegisterWaiter(ocrDataBlock_t *self, ocrFatGuid_t waiter, u32 slot,
                         bool isDepAdd) {
    ASSERT(0);
    return OCR_ENOSYS;
}

u8 lockableUnregisterWaiter(ocrDataBlock_t *self, ocrFatGuid_t waiter, u32 slot,
                           bool isDepRem) {
    ASSERT(0);
    return OCR_ENOSYS;
}

u8 newDataBlockLockable(ocrDataBlockFactory_t *factory, ocrFatGuid_t *guid, ocrFatGuid_t allocator,
                        ocrFatGuid_t allocPD, u64 size, void* ptr, ocrHint_t *hint, u32 flags,
                        ocrParamList_t *perInstance) {
    ocrPolicyDomain_t *pd = NULL;
    u8 returnValue = 0;
    ocrGuid_t resultGuid = NULL_GUID;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, NULL, &msg);

    ocrDataBlockLockable_t *result = NULL;
    u32 hintc = (flags & DB_PROP_NO_HINT) ? 0 : OCR_HINT_COUNT_DB_LOCKABLE;
    u32 mSize = sizeof(ocrDataBlockLockable_t) + hintc*sizeof(u64);

    if (flags & DB_PROP_RT_PROXY) {
        result = (ocrDataBlockLockable_t*)pd->fcts.pdMalloc(pd, mSize);
        result->base.guid = NULL_GUID;
    } else {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
        msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid) = *guid;
        PD_MSG_FIELD_I(size) = mSize;
        PD_MSG_FIELD_I(kind) = OCR_GUID_DB;
        PD_MSG_FIELD_I(properties) = flags & GUID_PROP_ALL;

        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));

        result = (ocrDataBlockLockable_t*)PD_MSG_FIELD_IO(guid.metaDataPtr);
        resultGuid = PD_MSG_FIELD_IO(guid.guid);
        returnValue = PD_MSG_FIELD_O(returnDetail);
#undef PD_MSG
#undef PD_TYPE
    }

    if(returnValue != 0) {
        return returnValue;
    }
    ASSERT(result);
    result->base.allocator = allocator.guid;
    result->base.allocatingPD = allocPD.guid;
    result->base.size = size;
    result->base.ptr = ptr;
    result->base.fctId = factory->factoryId;
    // Only keep flags that represent the nature of
    // the DB as opposed to one-time usage creation flags
    result->base.flags = (flags & (DB_PROP_SINGLE_ASSIGNMENT | DB_PROP_RT_PROXY));
    result->lock = 0;
    result->attributes.flags = result->base.flags;
    result->attributes.numUsers = 0;
    result->attributes.internalUsers = 0;
    result->attributes.freeRequested = 0;
    result->attributes.modeLock = DB_LOCKED_NONE;
    result->ewWaiterList = NULL;
    result->roWaiterList = NULL;
    result->itwWaiterList = NULL;
    result->itwLocation = INVALID_LOCATION;
    result->worker = NULL;

    if (hintc == 0) {
        result->hint.hintMask = 0;
        result->hint.hintVal = NULL;
    } else {
        OCR_RUNTIME_HINT_MASK_INIT(result->hint.hintMask, OCR_HINT_DB_T, factory->factoryId);
        result->hint.hintVal = (u64*)((u64)result + sizeof(ocrDataBlockLockable_t));
    }
#ifdef OCR_ENABLE_STATISTICS
    ocrTask_t *task = NULL;
    getCurrentEnv(NULL, NULL, &task, NULL);
    statsDB_CREATE(pd, task->guid, task, allocator.guid,
                   (ocrAllocator_t*)allocator.metaDataPtr, result->base.guid,
                   &(result->base));
#endif /* OCR_ENABLE_STATISTICS */

    DPRINTF(DEBUG_LVL_VERB, "Creating a datablock of size %"PRIu64", @ 0x%"PRIx64" (GUID: "GUIDF")\n",
            size, (u64)result->base.ptr, GUIDA(result->base.guid));
    OCR_TOOL_TRACE(false, OCR_TRACE_TYPE_DATABLOCK, OCR_ACTION_CREATE, size);

    // Do this at the very end; it indicates that the object
    // is actually valid
    hal_fence();
    result->base.guid = resultGuid;

    guid->guid = resultGuid;
    guid->metaDataPtr = result;
    return 0;
}

u8 lockableSetHint(ocrDataBlock_t* self, ocrHint_t *hint) {
    ocrDataBlockLockable_t *derived = (ocrDataBlockLockable_t*)self;
    ocrRuntimeHint_t *rHint = &(derived->hint);
    OCR_RUNTIME_HINT_SET(hint, rHint, OCR_HINT_COUNT_DB_LOCKABLE, ocrHintPropDbLockable, OCR_HINT_DB_PROP_START);
    return 0;
}

u8 lockableGetHint(ocrDataBlock_t* self, ocrHint_t *hint) {
    ocrDataBlockLockable_t *derived = (ocrDataBlockLockable_t*)self;
    ocrRuntimeHint_t *rHint = &(derived->hint);
    OCR_RUNTIME_HINT_GET(hint, rHint, OCR_HINT_COUNT_DB_LOCKABLE, ocrHintPropDbLockable, OCR_HINT_DB_PROP_START);
    return 0;
}

ocrRuntimeHint_t* getRuntimeHintDbLockable(ocrDataBlock_t* self) {
    ocrDataBlockLockable_t *derived = (ocrDataBlockLockable_t*)self;
    return &(derived->hint);
}

/******************************************************/
/* OCR DATABLOCK LOCKABLE FACTORY                      */
/******************************************************/

void destructLockableFactory(ocrDataBlockFactory_t *factory) {
    runtimeChunkFree((u64)factory->hintPropMap, PERSISTENT_CHUNK);
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrDataBlockFactory_t *newDataBlockFactoryLockable(ocrParamList_t *perType, u32 factoryId) {
    ocrDataBlockFactory_t *base = (ocrDataBlockFactory_t*)
                                  runtimeChunkAlloc(sizeof(ocrDataBlockFactoryLockable_t), PERSISTENT_CHUNK);

    base->instantiate = FUNC_ADDR(u8 (*)
                                  (ocrDataBlockFactory_t*, ocrFatGuid_t*, ocrFatGuid_t, ocrFatGuid_t,
                                   u64, void*, ocrHint_t*, u32, ocrParamList_t*), newDataBlockLockable);
    base->destruct = FUNC_ADDR(void (*)(ocrDataBlockFactory_t*), destructLockableFactory);
    base->fcts.destruct = FUNC_ADDR(u8 (*)(ocrDataBlock_t*), lockableDestruct);
    base->fcts.acquire = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, void**, ocrFatGuid_t, u32, ocrDbAccessMode_t, bool, u32), lockableAcquire);
    base->fcts.release = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrFatGuid_t, bool), lockableRelease);
    base->fcts.free = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrFatGuid_t, u32), lockableFree);
    base->fcts.registerWaiter = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrFatGuid_t,
                                                 u32, bool), lockableRegisterWaiter);
    base->fcts.unregisterWaiter = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrFatGuid_t,
                                                   u32, bool), lockableUnregisterWaiter);
    base->fcts.setHint = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrHint_t*), lockableSetHint);
    base->fcts.getHint = FUNC_ADDR(u8 (*)(ocrDataBlock_t*, ocrHint_t*), lockableGetHint);
    base->fcts.getRuntimeHint = FUNC_ADDR(ocrRuntimeHint_t* (*)(ocrDataBlock_t*), getRuntimeHintDbLockable);
    base->factoryId = factoryId;
    //Setup hint framework
    base->hintPropMap = (u64*)runtimeChunkAlloc(sizeof(u64)*(OCR_HINT_DB_PROP_END - OCR_HINT_DB_PROP_START - 1), PERSISTENT_CHUNK);
    OCR_HINT_SETUP(base->hintPropMap, ocrHintPropDbLockable, OCR_HINT_COUNT_DB_LOCKABLE, OCR_HINT_DB_PROP_START, OCR_HINT_DB_PROP_END);

    return base;
}
#endif /* ENABLE_DATABLOCK_LOCKABLE */
