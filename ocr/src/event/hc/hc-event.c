/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_EVENT_HC

#include "ocr-hal.h"
#include "debug.h"
#include "event/hc/hc-event.h"
#include "ocr-datablock.h"
#include "ocr-edt.h"
#include "ocr-event.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "utils/ocr-utils.h"
#include "ocr-worker.h"
#include "ocr-errors.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#include "ocr-statistics-callbacks.h"
#endif

#define SEALED_LIST ((void *) -1)
#define END_OF_LIST NULL
#define UNINITIALIZED_DATA ((ocrGuid_t) -2)

#define DEBUG_TYPE EVENT

/******************************************************/
/* OCR-HC Debug                                       */
/******************************************************/

#ifdef OCR_DEBUG
static char * eventTypeToString(ocrEvent_t * base) {
    ocrEventTypes_t type = base->kind;
    if(type == OCR_EVENT_ONCE_T) {
        return "once";
    } else if (type == OCR_EVENT_IDEM_T) {
        return "idem";
    } else if (type == OCR_EVENT_STICKY_T) {
        return "sticky";
    } else if (type == OCR_EVENT_LATCH_T) {
        return "latch";
    } else {
        return "unknown";
    }
}
#endif

/***********************************************************/
/* OCR-HC Event Hint Properties                             */
/* (Add implementation specific supported properties here) */
/***********************************************************/

u64 ocrHintPropEventHc[] = {
#ifdef ENABLE_HINTS
#endif
};

//Make sure OCR_HINT_COUNT_EVT_HC in hc-task.h is equal to the length of array ocrHintPropEventHc
ocrStaticAssert((sizeof(ocrHintPropEventHc)/sizeof(u64)) == OCR_HINT_COUNT_EVT_HC);
ocrStaticAssert(OCR_HINT_COUNT_EVT_HC < OCR_RUNTIME_HINT_PROP_BITS);

/******************************************************/
/* OCR-HC Events Implementation                       */
/******************************************************/


static u8 createDbRegNode(ocrFatGuid_t * dbFatGuid, u32 nbElems, bool doRelease, regNode_t ** node) {
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrTask_t *curTask = NULL;
    u32 i;
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t curEdt = {.guid = curTask!=NULL?curTask->guid:NULL_GUID, .metaDataPtr = curTask};
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_CREATE
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.type = PD_MSG_DB_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = (*dbFatGuid);
    PD_MSG_FIELD_IO(size) = sizeof(regNode_t)*nbElems;
    PD_MSG_FIELD_IO(properties) = DB_PROP_RT_ACQUIRE;
    PD_MSG_FIELD_I(edt) = curEdt;
    PD_MSG_FIELD_I(affinity.guid) = NULL_GUID;
    PD_MSG_FIELD_I(affinity.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(dbType) = RUNTIME_DBTYPE;
    PD_MSG_FIELD_I(allocator) = NO_ALLOC;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
    (*dbFatGuid) = PD_MSG_FIELD_IO(guid);
    regNode_t * temp = (regNode_t*) PD_MSG_FIELD_O(ptr);
    *node = temp;
    for(i = 0; i < nbElems; ++i) {
        temp[i].guid = UNINITIALIZED_GUID;
        temp[i].slot = 0;
        temp[i].mode = -1;
    }
#undef PD_TYPE
#define PD_TYPE PD_MSG_DB_RELEASE
    if (doRelease) {
        getCurrentEnv(NULL, NULL, NULL, &msg);
        msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid) = (*dbFatGuid);
        PD_MSG_FIELD_I(edt) = curEdt;
        PD_MSG_FIELD_I(ptr) = NULL;
        PD_MSG_FIELD_I(size) = 0;
        PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
        *node = NULL;
    }
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

//
// OCR-HC Single Events Implementation
//

u8 destructEventHc(ocrEvent_t *base) {

    ocrEventHc_t *event = (ocrEventHc_t*)base;
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrTask_t *curTask = NULL;
    getCurrentEnv(&pd, NULL, &curTask, &msg);


    DPRINTF(DEBUG_LVL_INFO, "Destroy %s: 0x%lx\n", eventTypeToString(base), base->guid,
            false, OCR_TRACE_TYPE_EVENT, OCR_ACTION_DESTROY);

#ifdef OCR_ENABLE_STATISTICS
    statsEVT_DESTROY(pd, getCurrentEDT(), NULL, base->guid, base);
#endif

    // Destroy datablocks linked with this event
    if (event->waitersDb.guid != UNINITIALIZED_GUID) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_FREE
        msg.type = PD_MSG_DB_FREE | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(guid) = event->waitersDb;
        PD_MSG_FIELD_I(edt.guid) = curTask ? curTask->guid : NULL_GUID;
        PD_MSG_FIELD_I(edt.metaDataPtr) = curTask;
        PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE | DB_PROP_NO_RELEASE;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));
#undef PD_MSG
#undef PD_TYPE
    }

    // Now destroy the GUID
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    // These next two statements may be not required. Just to be safe.
    PD_MSG_FIELD_I(guid.guid) = base->guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = base;
    PD_MSG_FIELD_I(properties) = 1; // Free metadata
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 destructEventHcPersist(ocrEvent_t *base) {
    ocrEventHc_t *event = (ocrEventHc_t*) base;
    // Addresses a race when the EDT that's satisfying the
    // event is still using the event's metadata but the children
    // EDT is has already invoked the destruct function.
    // NOTE: Current approach is to block. Progress is still ensured
    // because the satisfier is not blocking, it may just take time.
    // BUG #537 should improve on that by creating a lightweight
    // asynchronous operation to reschedule the destruction.
    if ((event->waitersCount) == -1) {
        // Satisfier is checked-in, wait for signal it is done.
        // Note that this is not addressing any other form of
        // illegal race conditions as defined per the specification
        while ((event->waitersCount) != ((u32) -2));
    }
    return destructEventHc(base);
}

ocrFatGuid_t getEventHc(ocrEvent_t *base) {
    ocrFatGuid_t res = {.guid = NULL_GUID, .metaDataPtr = NULL};
    switch(base->kind) {
    case OCR_EVENT_ONCE_T:
    case OCR_EVENT_LATCH_T:
        break;
    case OCR_EVENT_STICKY_T:
    case OCR_EVENT_IDEM_T: {
        ocrEventHcPersist_t *event = (ocrEventHcPersist_t*)base;
        res.guid = (event->data == UNINITIALIZED_GUID) ? ERROR_GUID : event->data;
        break;
    }
    default:
        ASSERT(0);
    }
    return res;
}

static u8 commonSatisfyRegNode(ocrPolicyDomain_t * pd, ocrPolicyMsg_t * msg,
                         ocrGuid_t evtGuid,
                         ocrFatGuid_t db, ocrFatGuid_t currentEdt,
                         regNode_t * node) {
#ifdef OCR_ENABLE_STATISTICS
    //TODO the null should be the base but it's a race
    statsDEP_SATISFYFromEvt(pd, evtGuid, NULL, node->guid,
                            db.guid, node->slot);
#endif
    DPRINTF(DEBUG_LVL_INFO, "SatisfyFromEvent: src: 0x%lx dst: 0x%lx\n", evtGuid, node->guid);
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
    getCurrentEnv(NULL, NULL, NULL, msg);
    msg->type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
    // Need to refill because out may overwrite some of the in fields
    PD_MSG_FIELD_I(satisfierGuid.guid) = evtGuid;
    // Passing NULL since base may become invalid
    PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(guid.guid) = node->guid;
    PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD_I(payload) = db;
    PD_MSG_FIELD_I(currentEdt) = currentEdt;
    PD_MSG_FIELD_I(slot) = node->slot;
    PD_MSG_FIELD_I(properties) = 0;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, msg, false));
#undef PD_MSG
#undef PD_TYPE

    return 0;
}

static u8 commonSatisfyWaiters(ocrPolicyDomain_t *pd, ocrEvent_t *base, ocrFatGuid_t db, u32 waitersCount,
                                ocrFatGuid_t currentEdt, ocrPolicyMsg_t * msg,
                                bool isPersistentEvent) {
    ocrEventHc_t * event = (ocrEventHc_t *) base;
    // waitersDb is safe to read because non-persistent event forbids further
    // registrations and persistent event registration is closed because of
    // event->waitersCount set to -1.
    ocrFatGuid_t dbWaiters = event->waitersDb;
    u32 i;
#if HCEVT_WAITER_STATIC_COUNT
    u32 ub = ((waitersCount < HCEVT_WAITER_STATIC_COUNT) ? waitersCount : HCEVT_WAITER_STATIC_COUNT);
    // Do static waiters first
    for(i = 0; i < ub; ++i) {
        RESULT_PROPAGATE(commonSatisfyRegNode(pd, msg, base->guid, db, currentEdt, &event->waiters[i]));
    }
    waitersCount -= ub;
#endif

    if(waitersCount > 0) {
        ASSERT(dbWaiters.guid != UNINITIALIZED_GUID);
        // First acquire the DB that contains the waiters
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
        msg->type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid) = dbWaiters;
        PD_MSG_FIELD_IO(edt) = currentEdt;
        PD_MSG_FIELD_IO(edtSlot) = EDT_SLOT_NONE;
        if (isPersistentEvent) {
            // !! Warning !! RW here (and not RO) works in pair with the lock
            // being unlocked before DB_RELEASE is called in 'registerWaiterEventHcPersist'
            PD_MSG_FIELD_IO(properties) = DB_MODE_RW | DB_PROP_RT_ACQUIRE;
        } else {
            PD_MSG_FIELD_IO(properties) = DB_MODE_CONST | DB_PROP_RT_ACQUIRE;
        }
        u8 res = pd->fcts.processMessage(pd, msg, true);
        ASSERT(!res);
        regNode_t * waiters = (regNode_t*)PD_MSG_FIELD_O(ptr);
        //BUG #273: related to 273: we should not get an updated deguidification...
        dbWaiters = PD_MSG_FIELD_IO(guid); //Get updated deguidifcation if needed
        ASSERT(waiters);
#undef PD_TYPE

        // Second, call satisfy on all the waiters
        for(i = 0; i < waitersCount; ++i) {
            RESULT_PROPAGATE(commonSatisfyRegNode(pd, msg, base->guid, db, currentEdt, &waiters[i]));
        }

        // Release the DB
#define PD_TYPE PD_MSG_DB_RELEASE
        getCurrentEnv(NULL, NULL, NULL, msg);
        msg->type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid) = dbWaiters;
        PD_MSG_FIELD_I(edt) = currentEdt;
        PD_MSG_FIELD_I(ptr) = NULL;
        PD_MSG_FIELD_I(size) = 0;
        PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, msg, true));
#undef PD_MSG
#undef PD_TYPE
    }

    return 0;
}

// For once events, we don't have to worry about
// concurrent registerWaiter calls (this would be a programmer error)
u8 satisfyEventHcOnce(ocrEvent_t *base, ocrFatGuid_t db, u32 slot) {
    ocrEventHc_t *event = (ocrEventHc_t*)base;
    ASSERT(slot == 0); // For non-latch events, only one slot

    DPRINTF(DEBUG_LVL_INFO, "Satisfy: 0x%lx with 0x%lx\n", base->guid, db.guid);

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t currentEdt;
    currentEdt.guid = (curTask == NULL) ? NULL_GUID : curTask->guid;
    currentEdt.metaDataPtr = curTask;
    u32 waitersCount = event->waitersCount;
    // This is only to help users find out about wrongful use of events
    event->waitersCount = (u32)-1; // Indicate that the event is satisfied

#ifdef OCR_ENABLE_STATISTICS
    statsDEP_SATISFYToEvt(pd, currentEdt.guid, NULL, base->guid, base, data, slot);
#endif

    if (waitersCount) {
        RESULT_PROPAGATE(commonSatisfyWaiters(pd, base, db, waitersCount, currentEdt, &msg, false));
    }

    // Since this a ONCE event, we need to destroy it as well
    // This is safe to do so at this point as all the messages have been sent
    return destructEventHc(base);
}

static u8 commonSatisfyEventHcPersist(ocrEvent_t *base, ocrFatGuid_t db, u32 slot, u32 waitersCount) {
    ocrEventHc_t * event = (ocrEventHc_t*) base;
    ASSERT(slot == 0); // Persistent-events are single slot
    DPRINTF(DEBUG_LVL_INFO, "Satisfy %s: 0x%lx with 0x%lx\n", eventTypeToString(base),
            base->guid, db.guid);

#ifdef OCR_ENABLE_STATISTICS
    ocrPolicyDomain_t *pd = getCurrentPD();
    ocrGuid_t edt = getCurrentEDT();
    statsDEP_SATISFYToEvt(pd, edt, NULL, base->guid, base, data, slot);
#endif

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t currentEdt;
    currentEdt.guid = (curTask == NULL) ? NULL_GUID : curTask->guid;
    currentEdt.metaDataPtr = curTask;
    // Process waiters to be satisfied
    if(waitersCount) {
        RESULT_PROPAGATE(commonSatisfyWaiters(pd, base, db, waitersCount, currentEdt, &msg, true));
    }

    hal_fence();// make sure all operations are done
    // This is to signal a concurrent destruct currently
    // waiting on the satisfaction being done it can now
    // proceed.
    event->waitersCount = (u32)-2;

    return 0;
}

// For idempotent events
u8 satisfyEventHcPersistIdem(ocrEvent_t *base, ocrFatGuid_t db, u32 slot) {
    ocrEventHc_t * event = (ocrEventHc_t*) base;
    u32 waitersCount;
    hal_lock32(&(event->waitersLock));
    if ((event->waitersCount == (u32)-1) || (event->waitersCount == (u32)-2))  {
        hal_unlock32(&(event->waitersLock));
        // Legal for idempotent to ignore subsequent satisfy
        return 1; //BUG #603 error codes: Put some error code here.
    } else {
        ((ocrEventHcPersist_t*)event)->data = db.guid;
        waitersCount = event->waitersCount;
        event->waitersCount = (u32)-1; // Indicate the event is satisfied
        hal_unlock32(&(event->waitersLock));
    }

    return commonSatisfyEventHcPersist(base, db, slot, waitersCount);
}

// For sticky event
u8 satisfyEventHcPersistSticky(ocrEvent_t *base, ocrFatGuid_t db, u32 slot) {
    ocrEventHc_t * event = (ocrEventHc_t*) base;
    hal_lock32(&(event->waitersLock));
    //BUG #809 Nanny-mode
    if (event->waitersCount == (u32)-1) {
        DPRINTF(DEBUG_LVL_WARN, "User-level error detected: try to satisfy a sticky event that's already satisfied\n");
        ASSERT(false);
        hal_unlock32(&(event->waitersLock));
        return 1; //BUG #603 error codes: Put some error code here.
    }
    ((ocrEventHcPersist_t*)event)->data = db.guid;
    u32 waitersCount = event->waitersCount;
    event->waitersCount = (u32)-1; // Indicate the event is satisfied
    hal_unlock32(&(event->waitersLock));

    return commonSatisfyEventHcPersist(base, db, slot, waitersCount);
}

// This is for latch events
u8 satisfyEventHcLatch(ocrEvent_t *base, ocrFatGuid_t db, u32 slot) {
    ocrEventHcLatch_t *event = (ocrEventHcLatch_t*)base;
    ASSERT(slot == OCR_EVENT_LATCH_DECR_SLOT ||
           slot == OCR_EVENT_LATCH_INCR_SLOT);

    s32 incr = (slot == OCR_EVENT_LATCH_DECR_SLOT)?-1:1;
    s32 count;
    do {
        count = event->counter;
    } while(hal_cmpswap32(&(event->counter), count, count+incr) != count);

    DPRINTF(DEBUG_LVL_INFO, "Satisfy %s: 0x%lx %s\n", eventTypeToString(base),
            base->guid, ((slot == OCR_EVENT_LATCH_DECR_SLOT) ? "decr":"incr"));

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t currentEdt;
    currentEdt.guid = (curTask == NULL) ? NULL_GUID : curTask->guid;
    currentEdt.metaDataPtr = curTask;

#ifdef OCR_ENABLE_STATISTICS
    statsDEP_SATISFYToEvt(pd, currentEdt.guid, NULL, base->guid, base, data, slot);
#endif
    if(count + incr != 0) {
        return 0;
    }
    // Here the event is satisfied
    DPRINTF(DEBUG_LVL_INFO, "Satisfy %s: 0x%lx reached zero\n", eventTypeToString(base), base->guid);

    u32 waitersCount = event->base.waitersCount;
    // This is only to help users find out about wrongful use of events
    event->base.waitersCount = (u32)-1; // Indicate that the event is satisfied

    if (waitersCount) {
        RESULT_PROPAGATE(commonSatisfyWaiters(pd, base, db, waitersCount, currentEdt, &msg, false));
    }

    // The latch is satisfied so we destroy it
    return destructEventHc(base);
}

u8 registerSignalerHc(ocrEvent_t *self, ocrFatGuid_t signaler, u32 slot,
                      ocrDbAccessMode_t mode, bool isDepAdd) {
    return 0; // We do not do anything for signalers
}

u8 unregisterSignalerHc(ocrEvent_t *self, ocrFatGuid_t signaler, u32 slot,
                        bool isDepRem) {
    return 0; // We do not do anything for signalers
}

static u8 commonEnqueueWaiter(ocrPolicyDomain_t *pd, ocrEvent_t *base, ocrFatGuid_t waiter,
                              u32 slot, ocrFatGuid_t currentEdt, ocrPolicyMsg_t * msg) {
    // Warn: Caller must have acquired the waitersLock
    ocrEventHc_t *event = (ocrEventHc_t*)base;
    u32 waitersCount = event->waitersCount;

#if HCEVT_WAITER_STATIC_COUNT
    if (waitersCount < HCEVT_WAITER_STATIC_COUNT) {
        event->waiters[waitersCount].guid = waiter.guid;
        event->waiters[waitersCount].slot = slot;
        ++event->waitersCount;
        // We can release the lock now
        hal_unlock32(&(event->waitersLock));
    } else {
#endif
        ocrFatGuid_t oldDbGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
        ocrFatGuid_t dbGuid = {.guid = NULL_GUID, .metaDataPtr = NULL};
        regNode_t *waiters = NULL;
        regNode_t *waitersNew = NULL;
        u8 toReturn = 0;
        // We're working with the dynamically created waiter list
        if (waitersCount == HCEVT_WAITER_STATIC_COUNT) {
            // Initial setup
            u8 toReturn = createDbRegNode(&(event->waitersDb), HCEVT_WAITER_DYNAMIC_COUNT, false, &waiters);
            if (toReturn) {
                ASSERT(false && "Failed allocating db waiter");
                hal_unlock32(&(event->waitersLock));
                return toReturn;
            }
            dbGuid = event->waitersDb; // for release
            event->waitersMax += HCEVT_WAITER_DYNAMIC_COUNT;
            waitersCount=0;
        } else {
            // Acquire the DB that contains the waiters
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
            getCurrentEnv(NULL, NULL, NULL, msg);
            msg->type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
            PD_MSG_FIELD_IO(guid) = event->waitersDb;
            PD_MSG_FIELD_IO(edt) = currentEdt;
            PD_MSG_FIELD_IO(edtSlot) = EDT_SLOT_NONE;
            PD_MSG_FIELD_IO(properties) = DB_MODE_RW | DB_PROP_RT_ACQUIRE;
            //Should be a local DB
            if((toReturn = pd->fcts.processMessage(pd, msg, true))) {
                // should be the only writer active on the waiter DB since we have the lock
                ASSERT(false); // debug
                ASSERT(toReturn != OCR_EBUSY);
                hal_unlock32(&(event->waitersLock));
                return toReturn; //BUG #603 error codes
            }
            waiters = (regNode_t*)PD_MSG_FIELD_O(ptr);
            //BUG #273
            event->waitersDb = PD_MSG_FIELD_IO(guid);
            ASSERT(waiters);
#undef PD_TYPE
            if(waitersCount + 1 == event->waitersMax) {
                // We need to create a new DB and copy things over
#define PD_TYPE PD_MSG_DB_CREATE
                getCurrentEnv(NULL, NULL, NULL, msg);
                msg->type = PD_MSG_DB_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                PD_MSG_FIELD_IO(guid) = dbGuid;
                PD_MSG_FIELD_IO(properties) = DB_PROP_RT_ACQUIRE;
                PD_MSG_FIELD_IO(size) = sizeof(regNode_t)*event->waitersMax*2;
                PD_MSG_FIELD_I(edt) = currentEdt;
                PD_MSG_FIELD_I(affinity.guid) = NULL_GUID;
                PD_MSG_FIELD_I(affinity.metaDataPtr) = NULL;
                PD_MSG_FIELD_I(dbType) = RUNTIME_DBTYPE;
                PD_MSG_FIELD_I(allocator) = NO_ALLOC;
                if((toReturn = pd->fcts.processMessage(pd, msg, true))) {
                    ASSERT(false); // debug
                    hal_unlock32(&(event->waitersLock));
                    return toReturn; //BUG #603 error codes
                }
                waitersNew = (regNode_t*)PD_MSG_FIELD_O(ptr);
                oldDbGuid = event->waitersDb;
                dbGuid = PD_MSG_FIELD_IO(guid);
                event->waitersDb = dbGuid;
#undef PD_TYPE
                // -HCEVT_WAITER_STATIC_COUNT because part of the waiters are in the statically allocated waiter array
                u32 nbNodes = waitersCount-HCEVT_WAITER_STATIC_COUNT;
                hal_memCopy(waitersNew, waiters, sizeof(regNode_t)*(nbNodes), false);
                event->waitersMax *= 2;
                u32 i;
                u32 maxNbNodes = event->waitersMax-HCEVT_WAITER_STATIC_COUNT;
                for(i = nbNodes; i < maxNbNodes; ++i) {
                    waitersNew[i].guid = NULL_GUID;
                    waitersNew[i].slot = 0;
                    waitersNew[i].mode = -1;
                }
                waiters = waitersNew;
            } else {
                dbGuid = event->waitersDb; // for release
            }
            waitersCount=event->waitersCount-HCEVT_WAITER_STATIC_COUNT;
        }

        waiters[waitersCount].guid = waiter.guid;
        waiters[waitersCount].slot = slot;
        ++event->waitersCount;

        // We can release the lock now
        hal_unlock32(&(event->waitersLock));

        // Release the waiter datablock / free old waiter DB when necessary
        //
        // In both cases it is important to release the GUID read from the cached
        // DB value and not from the event data-structure since we're operating
        // outside the lock there can be a new db created and assigned before getting here
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DB_RELEASE
        getCurrentEnv(NULL, NULL, NULL, msg);
        msg->type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid) = dbGuid;
        PD_MSG_FIELD_I(edt) = currentEdt;
        PD_MSG_FIELD_I(ptr) = NULL;
        PD_MSG_FIELD_I(size) = 0;
        PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, msg, true));
#undef PD_MSG
#undef PD_TYPE

        if(waitersNew) {
            // We need to free the old DB (implicitely released as DB_PROP_NO_RELEASE
            // is not provided here) and release the new one.
#define PD_MSG (msg)
#define PD_TYPE PD_MSG_DB_FREE
            getCurrentEnv(NULL, NULL, NULL, msg);
            msg->type = PD_MSG_DB_FREE | PD_MSG_REQUEST;
            PD_MSG_FIELD_I(guid) = oldDbGuid;
            PD_MSG_FIELD_I(edt) = currentEdt;
            PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
            if((toReturn = pd->fcts.processMessage(pd, msg, false))) {
                ASSERT(false); // debug
                return toReturn; //BUG #603 error codes
            }
#undef PD_MSG
#undef PD_TYPE
        }
#if HCEVT_WAITER_STATIC_COUNT
    }
#endif
    return 0; //Require registerSignaler invocation
}



/**
 * In this call, we do not contend with the satisfy (once and latch events) however,
 * we do contend with multiple registration.
 * By construction, users must ensure a ONCE event is registered before satisfy is called.
 */
u8 registerWaiterEventHc(ocrEvent_t *base, ocrFatGuid_t waiter, u32 slot, bool isDepAdd) {
    // Here we always add the waiter to our list so we ignore isDepAdd
    ocrEventHc_t *event = (ocrEventHc_t*)base;

    DPRINTF(DEBUG_LVL_INFO, "Register waiter %s: 0x%lx with waiter 0x%lx on slot %d\n",
            eventTypeToString(base), base->guid, waiter.guid, slot);

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    //BUG #809 this should be part of the n
    if (event->waitersCount == (u32)-1) {
        // This is best effort race check
        DPRINTF(DEBUG_LVL_WARN, "User-level error detected: try to register on a non-persistent event already satisfied\n");
        ASSERT(false);
        return 1; //BUG #603 error codes: Put some error code here.
    }
    ocrFatGuid_t currentEdt = {.guid = curTask!=NULL?curTask->guid:NULL_GUID, .metaDataPtr = curTask};
    hal_lock32(&(event->waitersLock)); // Lock is released by commonEnqueueWaiter
    return commonEnqueueWaiter(pd, base, waiter, slot, currentEdt, &msg);
}


/**
 * @brief Registers waiters on persistent events such as sticky or idempotent.
 *
 * This code contends with a satisfy call and with concurrent add-dependences that try
 * to register their waiter.
 * The event waiterLock is grabbed, if the event is already satisfied, directly satisfy
 * the waiter. Otherwise add the waiter's guid to the waiters db list. If db is too small
 * reallocate and copy over to a new one.
 *
 * Returns non-zero if the registerWaiter requires registerSignaler to be called there-after
 */
u8 registerWaiterEventHcPersist(ocrEvent_t *base, ocrFatGuid_t waiter, u32 slot, bool isDepAdd) {
    ocrEventHcPersist_t *event = (ocrEventHcPersist_t*)base;

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t currentEdt;
    currentEdt.guid = (curTask == NULL) ? NULL_GUID : curTask->guid;
    currentEdt.metaDataPtr = curTask;

    // EDTs incrementally register on their dependences as elements
    // get satisfied (Guarantees O(n) traversal of dependence list).
    // Other events register when the dependence is added
    ocrGuidKind waiterKind = OCR_GUID_NONE;
    RESULT_ASSERT(guidKind(pd, waiter, &waiterKind), ==, 0);

    if(isDepAdd && waiterKind == OCR_GUID_EDT) {
        ASSERT(false && "Should never happen anymore");
        // If we're adding a dependence and the waiter is an EDT we
        // skip this part. The event is registered on the EDT and
        // the EDT will register on the event only when its dependence
        // frontier reaches this event.
        return 0; //Require registerSignaler invocation
    }
    ASSERT(waiterKind == OCR_GUID_EDT || (waiterKind & OCR_GUID_EVENT));

    DPRINTF(DEBUG_LVL_INFO, "Register waiter %s: 0x%lx with waiter 0x%lx on slot %d\n",
            eventTypeToString(base), base->guid, waiter.guid, slot);
    hal_lock32(&(event->base.waitersLock));

    if(event->data != UNINITIALIZED_GUID) {
        hal_unlock32(&(event->base.waitersLock));
        // We send a message saying that we satisfy whatever tried to wait on us
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DEP_SATISFY
        msg.type = PD_MSG_DEP_SATISFY | PD_MSG_REQUEST;
        PD_MSG_FIELD_I(satisfierGuid.guid) = base->guid;
        PD_MSG_FIELD_I(satisfierGuid.metaDataPtr) = base;
        PD_MSG_FIELD_I(guid) = waiter;
        PD_MSG_FIELD_I(payload.guid) = event->data;
        PD_MSG_FIELD_I(payload.metaDataPtr) = NULL;
        PD_MSG_FIELD_I(currentEdt) = currentEdt;
        PD_MSG_FIELD_I(slot) = slot;
        PD_MSG_FIELD_I(properties) = 0;
        //BUG #603 error codes
        RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, false));
#undef PD_MSG
#undef PD_TYPE
        return 0; //Require registerSignaler invocation
    }

    // Lock is released by commonEnqueueWaiter
    return commonEnqueueWaiter(pd, base, waiter, slot, currentEdt, &msg);
}

// In this call, we do not contend with satisfy
u8 unregisterWaiterEventHc(ocrEvent_t *base, ocrFatGuid_t waiter, u32 slot, bool isDepRem) {
    // Always search for the waiter because we don't know if it registered or not so
    // ignore isDepRem
    ocrEventHc_t *event = (ocrEventHc_t*)base;


    DPRINTF(DEBUG_LVL_INFO, "UnRegister waiter %s: 0x%lx with waiter 0x%lx on slot %d\n",
            eventTypeToString(base), base->guid, waiter.guid, slot);

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    regNode_t *waiters = NULL;
    u32 i;

    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t curEdt = {.guid = curTask!=NULL?curTask->guid:NULL_GUID, .metaDataPtr = curTask};

    // Acquire the DB that contains the waiters
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
    msg.type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = event->waitersDb;
    PD_MSG_FIELD_IO(edt) = curEdt;
    PD_MSG_FIELD_IO(edtSlot) = EDT_SLOT_NONE;
    PD_MSG_FIELD_IO(properties) = DB_MODE_RW | DB_PROP_RT_ACQUIRE;
    //Should be a local DB
    u8 res = pd->fcts.processMessage(pd, &msg, true);
    ASSERT(!res); // Possible corruption of waitersDb

    waiters = (regNode_t*)PD_MSG_FIELD_O(ptr);
    //BUG #273
    event->waitersDb = PD_MSG_FIELD_IO(guid);
    ASSERT(waiters);
#undef PD_TYPE
    // We search for the waiter that we need and remove it
    for(i = 0; i < event->waitersCount; ++i) {
        if(waiters[i].guid == waiter.guid && waiters[i].slot == slot) {
            // We will copy all the other ones
            hal_memCopy((void*)&waiters[i], (void*)&waiters[i+1],
                        sizeof(regNode_t)*(event->waitersCount - i - 1), false);
            --event->waitersCount;
            break;
        }
    }

    // We always release waitersDb
#define PD_TYPE PD_MSG_DB_RELEASE
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = event->waitersDb;
    PD_MSG_FIELD_I(edt) = curEdt;
    PD_MSG_FIELD_I(ptr) = NULL;
    PD_MSG_FIELD_I(size) = 0;
    PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}


// In this call, we can have concurrent satisfy
u8 unregisterWaiterEventHcPersist(ocrEvent_t *base, ocrFatGuid_t waiter, u32 slot) {
    ocrEventHcPersist_t *event = (ocrEventHcPersist_t*)base;


    DPRINTF(DEBUG_LVL_INFO, "Unregister waiter %s: 0x%lx with waiter 0x%lx on slot %d\n",
            eventTypeToString(base), base->guid, waiter.guid, slot);

    ocrPolicyDomain_t *pd = NULL;
    ocrTask_t *curTask = NULL;
    PD_MSG_STACK(msg);
    regNode_t *waiters = NULL;
    u32 i;
    u8 toReturn = 0;

    getCurrentEnv(&pd, NULL, &curTask, &msg);
    ocrFatGuid_t curEdt = {.guid = curTask!=NULL?curTask->guid:NULL_GUID, .metaDataPtr = curTask};
    hal_lock32(&(event->base.waitersLock));
    if(event->data != UNINITIALIZED_GUID) {
        // We don't really care at this point so we don't do anything
        hal_unlock32(&(event->base.waitersLock));
        return 0;
    }

    // Here we need to actually update our waiter list. We still hold the lock
    // Acquire the DB that contains the waiters
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
    msg.type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = event->base.waitersDb;
    PD_MSG_FIELD_IO(edt) = curEdt;
    PD_MSG_FIELD_IO(edtSlot) = EDT_SLOT_NONE;
    PD_MSG_FIELD_IO(properties) = DB_MODE_RW | DB_PROP_RT_ACQUIRE;
    //Should be a local DB
    if((toReturn = pd->fcts.processMessage(pd, &msg, true))) {
        ASSERT(!toReturn); // Possible corruption of waitersDb
        hal_unlock32(&(event->base.waitersLock));
        return toReturn;
    }
    //BUG #273: Guid reading
    waiters = (regNode_t*)PD_MSG_FIELD_O(ptr);
    event->base.waitersDb = PD_MSG_FIELD_IO(guid);
    ASSERT(waiters);
#undef PD_TYPE
    // We search for the waiter that we need and remove it
    for(i = 0; i < event->base.waitersCount; ++i) {
        if(waiters[i].guid == waiter.guid && waiters[i].slot == slot) {
            // We will copy all the other ones
            hal_memCopy((void*)&waiters[i], (void*)&waiters[i+1],
                        sizeof(regNode_t)*(event->base.waitersCount - i - 1), false);
            --event->base.waitersCount;
            break;
        }
    }

    // We can release the lock now
    hal_unlock32(&(event->base.waitersLock));

    // We always release waitersDb
#define PD_TYPE PD_MSG_DB_RELEASE
    getCurrentEnv(NULL, NULL, NULL, &msg);
    msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = event->base.waitersDb;
    PD_MSG_FIELD_I(edt) = curEdt;
    PD_MSG_FIELD_I(ptr) = NULL;
    PD_MSG_FIELD_I(size) = 0;
    PD_MSG_FIELD_I(properties) = DB_PROP_RT_ACQUIRE;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
#undef PD_MSG
#undef PD_TYPE
    return 0;
}

u8 setHintEventHc(ocrEvent_t* self, ocrHint_t *hint) {
    ocrEventHc_t *derived = (ocrEventHc_t*)self;
    ocrRuntimeHint_t *rHint = &(derived->hint);
    OCR_RUNTIME_HINT_SET(hint, rHint, OCR_HINT_COUNT_EVT_HC, ocrHintPropEventHc, OCR_HINT_EVT_PROP_START);
    return 0;
}

u8 getHintEventHc(ocrEvent_t* self, ocrHint_t *hint) {
    ocrEventHc_t *derived = (ocrEventHc_t*)self;
    ocrRuntimeHint_t *rHint = &(derived->hint);
    OCR_RUNTIME_HINT_GET(hint, rHint, OCR_HINT_COUNT_EVT_HC, ocrHintPropEventHc, OCR_HINT_EVT_PROP_START);
    return 0;
}

ocrRuntimeHint_t* getRuntimeHintEventHc(ocrEvent_t* self) {
    ocrEventHc_t *derived = (ocrEventHc_t*)self;
    return &(derived->hint);
}

/******************************************************/
/* OCR-HC Events Factory                              */
/******************************************************/

u8 newEventHc(ocrEventFactory_t * factory, ocrFatGuid_t *guid,
              ocrEventTypes_t eventType, u32 properties,
              ocrParamList_t *perInstance) {
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrTask_t *curTask = NULL;
    u8 returnValue = 0;

    getCurrentEnv(&pd, NULL, &curTask, &msg);
    // ocrFatGuid_t curEdt = {.guid = curTask!=NULL?curTask->guid:NULL_GUID, .metaDataPtr = curTask};

    // Create the event itself by getting a GUID
    u64 sizeOfGuid = sizeof(ocrEventHc_t);
    if(eventType == OCR_EVENT_LATCH_T) {
        sizeOfGuid = sizeof(ocrEventHcLatch_t);
    }
    if((eventType == OCR_EVENT_IDEM_T) || (eventType == OCR_EVENT_STICKY_T)) {
        sizeOfGuid = sizeof(ocrEventHcPersist_t);
    }
    u32 hintc = OCR_HINT_COUNT_EVT_HC;

    ocrGuidKind kind;
    switch(eventType) {
        case OCR_EVENT_ONCE_T:
            kind = OCR_GUID_EVENT_ONCE;
            break;
        case OCR_EVENT_IDEM_T:
            kind = OCR_GUID_EVENT_IDEM;
            break;
        case OCR_EVENT_STICKY_T:
            kind = OCR_GUID_EVENT_STICKY;
            break;
        case OCR_EVENT_LATCH_T:
            kind = OCR_GUID_EVENT_LATCH;
            break;
        default:
            kind = OCR_GUID_NONE; // To keep clang happy
            ASSERT(false && "Unknown type of event");
    }
    ocrGuid_t resultGuid = NULL_GUID;
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_CREATE
    msg.type = PD_MSG_GUID_CREATE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD_IO(guid) = *guid;
    // We allocate everything in the meta-data to keep things simple
    PD_MSG_FIELD_I(size) = sizeOfGuid + hintc*sizeof(u64);
    PD_MSG_FIELD_I(kind) = kind;
    PD_MSG_FIELD_I(properties) = properties;
    RESULT_PROPAGATE(pd->fcts.processMessage(pd, &msg, true));
    ocrEventHc_t *event = (ocrEventHc_t*)PD_MSG_FIELD_IO(guid.metaDataPtr);
    ocrEvent_t *base = (ocrEvent_t*)event;
    returnValue = PD_MSG_FIELD_O(returnDetail);
    ASSERT(event);

    if(returnValue != 0) {
        return returnValue;
    }

    // Set-up base structures
    resultGuid = PD_MSG_FIELD_IO(guid.guid);
    base->kind = eventType;
    base->fctId = factory->factoryId;
#undef PD_MSG
#undef PD_TYPE

    // Set-up HC specific structures
    event->waitersCount = 0;
    event->waitersMax = HCEVT_WAITER_STATIC_COUNT;
    event->waitersLock = 0;

    int jj = 0;
    while (jj < HCEVT_WAITER_STATIC_COUNT) {
        event->waiters[jj].guid = NULL_GUID;
        event->waiters[jj].slot = 0;
        event->waiters[jj].mode = -1;
        jj++;
    }

    if(eventType == OCR_EVENT_LATCH_T) {
        // Initialize the counter
        ((ocrEventHcLatch_t*)event)->counter = 0;
    }
    if(eventType == OCR_EVENT_IDEM_T || eventType == OCR_EVENT_STICKY_T) {
        ((ocrEventHcPersist_t*)event)->data = UNINITIALIZED_GUID;
    }

    if (hintc == 0) {
        event->hint.hintMask = 0;
        event->hint.hintVal = NULL;
    } else {
        OCR_RUNTIME_HINT_MASK_INIT(event->hint.hintMask, OCR_HINT_EVT_T, factory->factoryId);
        event->hint.hintVal = (u64*)((u64)base + sizeOfGuid);
    }

    // Initialize GUIDs for the waiters data-blocks
    event->waitersDb.guid = UNINITIALIZED_GUID;
    event->waitersDb.metaDataPtr = NULL;

    // Do this at the very end; it indicates that the object
    // of the GUID is actually valid
    hal_fence(); // Make sure sure this really happens last
    base->guid = resultGuid;

    DPRINTF(DEBUG_LVL_INFO, "Create %s: 0x%lx\n", eventTypeToString(base), base->guid);
#ifdef OCR_ENABLE_STATISTICS
    statsEVT_CREATE(getCurrentPD(), getCurrentEDT(), NULL, base->guid, base);
#endif
    if(returnValue == 0) {
        guid->guid = base->guid;
        guid->metaDataPtr = base;
    }
    return returnValue;
}

void destructEventFactoryHc(ocrEventFactory_t * factory) {
    runtimeChunkFree((u64)factory->hintPropMap, PERSISTENT_CHUNK);
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrEventFactory_t * newEventFactoryHc(ocrParamList_t *perType, u32 factoryId) {
    ocrEventFactory_t* base = (ocrEventFactory_t*) runtimeChunkAlloc(
                                  sizeof(ocrEventFactoryHc_t), PERSISTENT_CHUNK);

    base->instantiate = FUNC_ADDR(u8 (*)(ocrEventFactory_t*, ocrFatGuid_t*,
                                  ocrEventTypes_t, u32, ocrParamList_t*), newEventHc);
    base->destruct =  FUNC_ADDR(void (*)(ocrEventFactory_t*), destructEventFactoryHc);
    // Initialize the function pointers

    // Setup common functions
    base->commonFcts.setHint = FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrHint_t*), setHintEventHc);
    base->commonFcts.getHint = FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrHint_t*), getHintEventHc);
    base->commonFcts.getRuntimeHint = FUNC_ADDR(ocrRuntimeHint_t* (*)(ocrEvent_t*), getRuntimeHintEventHc);

    // Setup functions properly
    u32 i;
    for(i = 0; i < (u32)OCR_EVENT_T_MAX; ++i) {
        base->fcts[i].destruct = FUNC_ADDR(u8 (*)(ocrEvent_t*), destructEventHc);
        base->fcts[i].get = FUNC_ADDR(ocrFatGuid_t (*)(ocrEvent_t*), getEventHc);
        base->fcts[i].registerSignaler = FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, ocrDbAccessMode_t, bool), registerSignalerHc);
        base->fcts[i].unregisterSignaler = FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, bool), unregisterSignalerHc);
    }
    // Setup satisfy function pointers
    base->fcts[OCR_EVENT_ONCE_T].satisfy =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32), satisfyEventHcOnce);
    base->fcts[OCR_EVENT_LATCH_T].satisfy =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32), satisfyEventHcLatch);
    base->fcts[OCR_EVENT_IDEM_T].satisfy =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32), satisfyEventHcPersistIdem);
    base->fcts[OCR_EVENT_STICKY_T].satisfy =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32), satisfyEventHcPersistSticky);

    // Setup registration function pointers
    base->fcts[OCR_EVENT_ONCE_T].registerWaiter =
    base->fcts[OCR_EVENT_LATCH_T].registerWaiter =
         FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, bool), registerWaiterEventHc);
    base->fcts[OCR_EVENT_IDEM_T].registerWaiter =
    base->fcts[OCR_EVENT_STICKY_T].registerWaiter =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, bool), registerWaiterEventHcPersist);

    base->fcts[OCR_EVENT_ONCE_T].unregisterWaiter =
    base->fcts[OCR_EVENT_LATCH_T].unregisterWaiter =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, bool), unregisterWaiterEventHc);
    base->fcts[OCR_EVENT_IDEM_T].unregisterWaiter =
    base->fcts[OCR_EVENT_STICKY_T].unregisterWaiter =
        FUNC_ADDR(u8 (*)(ocrEvent_t*, ocrFatGuid_t, u32, bool), unregisterWaiterEventHcPersist);

    base->factoryId = factoryId;

    //Setup hint framework
    base->hintPropMap = (u64*)runtimeChunkAlloc(sizeof(u64)*(OCR_HINT_EVT_PROP_END - OCR_HINT_EVT_PROP_START - 1), PERSISTENT_CHUNK);
    OCR_HINT_SETUP(base->hintPropMap, ocrHintPropEventHc, OCR_HINT_COUNT_EVT_HC, OCR_HINT_EVT_PROP_START, OCR_HINT_EVT_PROP_END);
    return base;
}
#endif /* ENABLE_EVENT_HC */
