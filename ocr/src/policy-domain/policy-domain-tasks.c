/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "utils/ocr-utils.h"
#include "ocr-policy-domain-tasks.h"

/* Use a separate type of debug-type just for micro-tasks */
#define DEBUG_TYPE MICROTASKS


/***************************************/
/********** Internal functions *********/
/***************************************/

// Macros to help with code factoring
#define CHECK_MALLOC(expr, cleanup) do {    \
    if((expr) == NULL) {            \
        DPRINTF(DEBUG_LVL_WARN, "Cannot allocate memory for \"" #expr "\"\n");   \
        toReturn = OCR_ENOMEM;      \
        cleanup;                    \
        ASSERT(false);              \
        goto _END_FUNC;             \
    }                               \
} while(0);

// WARNING: CHECK_RESULT looks for the result to be 0. So it works the opposite
// of an assert. If result is 0, all will be good, otherwise it will error out.
// This is meant to check the return value of functions
#define CHECK_RESULT(expr, cleanup, newcode) do {   \
    if((expr) != 0) {               \
        DPRINTF(DEBUG_LVL_WARN, "Error in check \"" #expr "\"; aborting\n");   \
        cleanup;                    \
        newcode;                    \
        ASSERT(false);              \
        goto _END_FUNC;             \
    }                               \
} while(0);

#define CHECK_RESULT_T(expr, cleanup, newcode) CHECK_RESULT(!(expr), cleanup, newcode)

#define END_LABEL(label) label: __attribute__((unused));


#define PROPAGATE_UP_TREE(node, parent, cond, actions) do {      \
    while (((parent) != NULL) && (cond)) {                       \
        hal_lock32(&((parent)->lock));                                  \
        DPRINTF(DEBUG_LVL_VVERB, "BEFORE: %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        actions;                                                        \
        DPRINTF(DEBUG_LVL_VVERB, "AFTER:  %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        hal_unlock32(&(curNode->lock));                                 \
        curNode = parent;                                               \
        parent = curNode->parent;                                       \
    }                                                                   \
    /* Unlock the final one */                                          \
    hal_unlock32(&(curNode->lock));                                     \
    } while(0);

#define PROPAGATE_UP_TREE_CHECK_LOCK(node, parent, cond, actions) do {  \
    bool _releaseLock = node->lock == 1;                                \
    while (((parent) != NULL) && (cond)) {                              \
        hal_lock32(&((parent)->lock));                                  \
        DPRINTF(DEBUG_LVL_VVERB, "BEFORE: %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        actions;                                                        \
        DPRINTF(DEBUG_LVL_VVERB, "AFTER:  %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        if(_releaseLock) hal_unlock32(&(curNode->lock));                \
        _releaseLock = true; /* Always lock/unlock up the chain */      \
        curNode = parent;                                               \
        parent = curNode->parent;                                       \
    }                                                                   \
    /* Unlock the final one (which can be the first one */              \
    if(_releaseLock) hal_unlock32(&(curNode->lock));                    \
    } while(0);

#define PROPAGATE_UP_TREE_NO_UNLOCK(node, parent, cond, actions) do {   \
    bool _releaseLock = false;                                          \
    while (((parent) != NULL) && (cond)) {                              \
        hal_lock32(&((parent)->lock));                                  \
        DPRINTF(DEBUG_LVL_VVERB, "BEFORE: %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        actions;                                                        \
        DPRINTF(DEBUG_LVL_VVERB, "AFTER:  %p -> %p [%"PRIu32"] "         \
                "curNode: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"], "              \
                "parent: [F;NP;R: 0x%"PRIx64"; 0x%"PRIx64"; 0x%"PRIx64"]\n",              \
                (node), (parent), (node)->parentSlot,                   \
                (node)->nodeFree, (node)->nodeNeedsProcess,             \
                (node)->nodeReady, (parent)->nodeFree,                  \
                (parent)->nodeNeedsProcess, (parent)->nodeReady);       \
        if(_releaseLock) hal_unlock32(&(curNode->lock));                \
        _releaseLock = true; /* Always lock/unlock up the chain */      \
        curNode = parent;                                               \
        parent = curNode->parent;                                       \
    }                                                                   \
    /* Unlock the final one (which can be the first one) */             \
    if(_releaseLock) hal_unlock32(&(curNode->lock));                    \
    } while(0);
// Size of the bit vector we use
#define BV_SIZE 64
#define BV_SIZE_LOG2 6

#define ctz(val) ctz64(val)

// ----- Action related functions -----

/**
 * @brief Processes and acts on a given action
 *
 * This call will process the action 'action'
 *
 * @param[in] pd            Policy domain to use
 * @param[in] strand        Strand this action is associated with
 * @param[in] action        Action to process
 * @param[in] properties    Properties (unused for now)
 * @return a status code:
 *    - 0: all went well
 *    - OCR_EINVAL: invalid value for action or properties
 */
static u8 _pdProcessAction(ocrPolicyDomain_t *pd, pdStrand_t *strand,
                           pdAction_t* action, u32 properties);


// ----- Strand related functions -----

/**
 * @brief Destroys (and frees) a table node
 *
 * This function goes down the subtree freeing everything it can
 *
 * @warn This function expects the node to be totally free
 *
 * @param[in] pd        Policy domain to use
 * @param[in] node      Node to destroy
 *
 * @return a status code:
 *     - 0 on success
 *     - OCR_EINVAL if node is NULL
 */
static u8 _pdDestroyStrandTableNode(ocrPolicyDomain_t *pd, pdStrandTableNode_t *node);


#define HAS_EXPECTED_VALUE  0x1
#define BLOCK               0x2
/**
 * @brief Attempts to grab a lock on a strand by switching its properties value
 *
 * If 'BLOCK' is set, this call will block until the lock can be acquired.
 * Otherwise, the call will attempt once to grab the lock (if it is free) and return
 * if it cannot
 *
 * @param[in] strand        Strand to grab the lock on
 * @param[in] expectedValue (optional) The expected value of the properties field
 *                          to lock
 * @param[in] properties    Combination of HAS_EXPECTED_VALUE and BLOCK
 * @return a status code:
 *     - 0: the lock was acquired
 *     - OCR_EBUSY: the lock could not be acquired (properties does not have BLOCK)
 *     - OCR_EINVAL: Invalid expectedValue (properties has HAS_EXPECTED_VALUE)
 * @warning If you use an expected value and pass a value that has PDST_LOCK in it,
 * this call will return OCR_EBUSY if you do not have BLOCK and OCR_EINVAL if you do
 */
static u8 _pdLockStrand(pdStrand_t *strand, u32 expectedValue, u32 properties);

#define IS_STRAND        0x1 /**< The node is a strand node */
#define IS_LEAF          0x2 /**< The node is a leaf node */
/**
 * @brief Initialize a strand table node
 *
 * This function initializes a strand table node setting its parent, initializing
 * it to be fully empty and creating the sub-nodes/leafs if numChildrenToInit is non
 * zero. This only applies to leaf nodes. This MUST be zero for non-leaf nodes.
 *
 * @param[in] pd                    Policy domain to use (can be NULL and will be resolved)
 * @param[in] node                  Node to initialize
 * @param[in] parent                Parent of the node or NULL if no parent
 * @param[in] parentSlot            Slot in the parent (ignored if parent is NULL)
 * @param[in] rdepth                Reverse depth of node. Is 0 for leaf nodes and goes up
 *                                  from there
  *                                  the leaf strand. Otherwise, must be 0
 * @param[in] numChildrenToInit     For leaf nodes, create this number of strands.
 *                                  Otherwise, must be 0
 * @param[in] flags                 Flags for creation: IS_LEAF
 *
 * @note This function does not grab any locks but requires a lock on node or exclusive
 * access to it
 *
 * @return a status code:
 *      - 0: successful
 *      - OCR_ENOMEM: insufficient memory
 *      - OCR_EINVAL: invalid numChildrenToInit or lock not held on parent
 */
static u8 _pdInitializeStrandTableNode(ocrPolicyDomain_t *pd, pdStrandTableNode_t *node,
                                       pdStrandTableNode_t *parent, u32 parentSlot,
                                       u32 level, u32 numChildrenToInit, u8 flags);


/**
 * @brief Sets the child of a strand node
 *
 * Sets the child tablenode or strand for a tablenode. There should be no
 * existing child at that index.
 *
 * @param[in] pd                    Policy domain
 * @param[in] parent                Parent to modify
 * @param[in] idx                   Index in the parent
 * @param[in] child                 Either a pdStrand_t or pdStrandTableNode_t
 * @param[in] flags                 Flags (IS_STRAND if the child is a strand)
 *
 * @note This function does not grab any locks but requires a lock on both the
 * parent and child (or ensure non-concurrency of accesses to parent and child)
 *
 * @return a status code:
 *      - 0: successful
 *      - OCR_EACCES: a child already exists at that index
 *      - OCR_EINVAL: child has the wrong parent
 */
static u8 _pdSetStrandNodeAtIdx(ocrPolicyDomain_t *pd, pdStrandTableNode_t *parent,
                                u32 idx, void* child, u8 flags);


/**
 * @brief Frees the strand at index 'index' in 'table'
 *
 * The strand must be locked prior to calling this function.
 *
 * @param[in] pd        Policy domain to use. Must not be NULL
  * @param[in] index     Strand to free
 * @return a status code:
 *      - 0: successful
 *      - OCR_EINVAL: index points to an invalid strand or strand is not locked
 */
static u8 _pdFreeStrand(ocrPolicyDomain_t* pd, pdStrand_t *strand);

/***************************************/
/***** pdEvent_t related functions *****/
/***************************************/

/**< Returns the strand table of a "fake" event pointer */
#define EVT_DECODE_ST_TBL(evt) ((evt) & 0x7)
/**< Rreturns the strand table index of a "fake" event pointer */
#define EVT_DECODE_ST_IDX(evt) ((evt) >> 3)

u8 pdCreateEvent(ocrPolicyDomain_t *pd, pdEvent_t **event, u32 type, u8 reserveInTable) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdCreateEvent(pd:%p, event**:%p [%p], type:%"PRIu32", table:%"PRIu32")\n",
            pd, event, *event, type, reserveInTable);

#define _END_FUNC createEventEnd
    ASSERT(event); // Cannot call if you don't want the event back.
    u8 toReturn = 0;
    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }
    u64 sizeToAllocate = 0;
    *event = NULL;
    switch (type) {
        case PDEVT_TYPE_BASIC:
            sizeToAllocate = sizeof(pdEvent_t);
            break;
        case PDEVT_TYPE_LIST:
            sizeToAllocate = sizeof(pdEventList_t);
            break;
        case PDEVT_TYPE_MERGE:
            sizeToAllocate = sizeof(pdEventMerge_t);
            break;
        case PDEVT_TYPE_MSG:
            sizeToAllocate = sizeof(pdEventMsg_t);
            break;
        default:
            DPRINTF(DEBUG_LVL_WARN, "PD Event type 0x%"PRIu32" not known\n", type);
            return OCR_EINVAL;
    }
    /* BUG #899: replace with proper slab allocator */
    CHECK_MALLOC(*event = (pdEvent_t*)pd->fcts.pdMalloc(pd, sizeToAllocate), );
    DPRINTF(DEBUG_LVL_VERB, "Allocated event of size %"PRIu64" -> ptr: %p\n",
            sizeToAllocate, *event);

    // Initialize base aspect
    (*event)->slabObj.userCount = 1;
    (*event)->properties = type;

    // TODO: Type specific initialization here
    switch (type) {
        case PDEVT_TYPE_BASIC:
            break;
        case PDEVT_TYPE_LIST:
            break;
        case PDEVT_TYPE_MERGE:
            break;
        case PDEVT_TYPE_MSG:
            break;
        default:
            break;
    }

    // Deal with insertion into the strands table if needed
    pdStrandTable_t *stTable = NULL;
    if(reserveInTable  && reserveInTable <= PDSTT_COMM) {
        DPRINTF(DEBUG_LVL_VERB, "Reserving slot in table %"PRIu32"\n", reserveInTable);
        // This means it is PDSTT_COMM or PDSTT_EVT so we look for the proper table
        stTable = pd->strandTables[reserveInTable - 1];
        pdStrand_t* myStrand = NULL;
        CHECK_RESULT(
            toReturn |= pdGetNewStrand(pd, &myStrand, stTable, *event, PDST_UHOLD),
            pd->fcts.pdFree(pd, *event),);
        DPRINTF(DEBUG_LVL_VERB, "Event %p has index %"PRIu64"\n", *event, (*event)->strand->index);
        // This assert failure indicates a coding issue in the runtime
        RESULT_ASSERT(pdUnlockStrand(myStrand), ==, 0);
    } else {
        DPRINTF(DEBUG_LVL_WARN, "Invalid value for reserveInTable: %"PRIu32"\n", reserveInTable);
        return OCR_EINVAL;
    }

END_LABEL(createEventEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdCreateEvent -> %"PRIu32"; event: %p; event->strand->index: %"PRIu64"\n",
            toReturn, *event, (*event)->strand->index);
    return toReturn;
#undef _END_FUNC
}

u8 pdResolveEvent(ocrPolicyDomain_t *pd, u64 *evtValue, u8 clearFwdHold) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdResolveEvent(pd:%p, evtValue*:%p [0x%"PRIx64"], clearHold:%"PRIu32")\n",
            pd, evtValue, *evtValue, clearFwdHold);
#define _END_FUNC resolveEventEnd

    u8 toReturn = 0;
    if(pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }
    u8 stTableIdx = EVT_DECODE_ST_TBL(*evtValue);
    if (stTableIdx) {
        DPRINTF(DEBUG_LVL_VERB, "Event = (table %"PRIu32", idx: %"PRIu64")\n",
                stTableIdx, EVT_DECODE_ST_IDX(*evtValue));
        // This is a pointer in a strands table
        pdStrandTable_t *stTable = NULL;
        if(stTableIdx < PDSTT_COMM) {
            stTable = pd->strandTables[stTableIdx-1];
        } else {
            DPRINTF(DEBUG_LVL_WARN, "Invalid event value: %"PRIu32" does not represent "
                    "a valid table (from event value 0x%"PRIx64")\n",
                    stTableIdx, *evtValue);
            return OCR_EINVAL;
        }
        DPRINTF(DEBUG_LVL_VERB, "Looking in table %p\n", stTable);
        pdStrand_t* myStrand = NULL;
        CHECK_RESULT(toReturn |= pdGetStrandForIndex(pd, &myStrand, stTable,
                                                     EVT_DECODE_ST_IDX(*evtValue)),,);

        // Here, we managed to get the strand properly
        // The pdGetStrandForIndex function will lock the strand, we can then
        // observe the state freely
        DPRINTF(DEBUG_LVL_VVERB, "Event 0x%"PRIx64" -> strand %p (props: 0x%"PRIx32")\n",
                *evtValue, myStrand, myStrand->properties);
        ASSERT(myStrand->properties & PDST_LOCK);
        if((myStrand->properties & PDST_WAIT) == 0) {
            // Event is ready
            // The following assert ensures that the event in the slot has
            // the slot's index. Failure indicates a runtime error
            ASSERT(myStrand->index == (*evtValue));
            DPRINTF(DEBUG_LVL_VERB, "Event 0x%"PRIx64" -> %p\n",
                    *evtValue, myStrand->curEvent);
            *evtValue = (u64)(myStrand->curEvent);
            if(clearFwdHold) {
                myStrand->properties &= ~PDST_RHOLD;
            }
            if((myStrand->properties & PDST_HOLD) == 0) {
                DPRINTF(DEBUG_LVL_VVERB, "Freeing strand %p [idx %"PRIu64"] after resolution\n",
                        myStrand, myStrand->index);
                RESULT_ASSERT(_pdFreeStrand(pd, ((pdEvent_t*)evtValue)->strand), ==, 0);
            } else {
                RESULT_ASSERT(pdUnlockStrand(myStrand), ==, 0);
            }
        } else {
            // The event is not ready
            DPRINTF(DEBUG_LVL_VERB, "Event 0x%"PRIx64" not ready\n", *evtValue);
            *evtValue = (u64)(myStrand->curEvent);
            RESULT_ASSERT(pdUnlockStrand(myStrand), ==, 0);
            toReturn = OCR_EBUSY;
        }
    } else {
        DPRINTF(DEBUG_LVL_VERB, "Event 0x%"PRIx64" is already a pointer\n", *evtValue);
        toReturn = OCR_ENOP;
    }
END_LABEL(resolveEventEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdResolveEvent -> %"PRIu32"; event: 0x%"PRIx64"\n",
            toReturn, *evtValue);
    return toReturn;
#undef _END_FUNC
}

u8 pdMarkReadyEvent(ocrPolicyDomain_t *pd, pdEvent_t *evt) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdMarkReadyEvent(pd:%p, evt:%p)\n",
            pd, evt);
#define _END_FUNC markReadyEventEnd

    u8 toReturn = 0;
    if(pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    CHECK_RESULT_T(evt != NULL, , toReturn = OCR_EINVAL);
    CHECK_RESULT_T((evt->properties & PDEVT_READY) == 0, , toReturn = OCR_EINVAL);

    evt->properties |= PDEVT_READY;
    if(evt->strand != NULL) {
        DPRINTF(DEBUG_LVL_VERB, "Event has strand %p -> going to update\n", evt->strand);
        // First grab the lock on the strand
        pdStrand_t *strand = evt->strand;
        u32 stIdx = strand->index & ((1<<BV_SIZE_LOG2) - 1);
        pdStrandTableNode_t *curNode = strand->parent;
        ASSERT(curNode);

        // Lock the strand
        RESULT_ASSERT(_pdLockStrand(strand, 0, BLOCK), ==, 0);

        // This should be the case since the event was not ready yet
        ASSERT((strand->properties & PDST_WAIT_EVT) != 0);
        strand->properties &= ~PDST_WAIT_EVT;

        // The following things can happen here
        //     - there are actions so this strand needs processing
        //     - there are no actions -> this strand becomes ready (destroyed or kept around)
        bool propagateReady = false, propagateNP = false, didFree = false;
        hal_lock32(&(curNode->lock));
        if ((strand->properties & PDST_WAIT_ACT) != 0) {
            DPRINTF(DEBUG_LVL_VERB, "Strand %p has waiting actions -> setting NP\n", strand);
            // We have pending actions, making this a NP node
            propagateNP = curNode->nodeNeedsProcess == 0ULL;
            curNode->nodeNeedsProcess |= (1ULL<<stIdx);
        } else {
            DPRINTF(DEBUG_LVL_VERB, "Strand %p is fully ready\n", strand);
            propagateReady = curNode->nodeReady == 0ULL;
            curNode->nodeReady |= (1ULL<<stIdx);
        }

        if((strand->properties & PDST_WAIT) == 0) {
            // Strand is ready; we either free it or keep it there due to a hold
            if ((strand->properties & PDST_HOLD) == 0) {
                // We can free the strand now
                DPRINTF(DEBUG_LVL_VERB, "(POSSIBLE RACE) Freeing strand %p [idx %"PRIu64"] after making event ready\n",
                        strand, strand->index);
                // We unset the nodeReady bit to prevent unecessary propagation
                curNode->nodeReady &= ~(1ULL<<stIdx);
                hal_unlock32(&(curNode->lock));
                RESULT_ASSERT(_pdFreeStrand(pd, strand), ==, 0);
                ASSERT(!propagateNP);
                propagateReady = false; // No need to change this since we freed the node
                didFree = true;
            } else {
                DPRINTF(DEBUG_LVL_VERB, "Strand %p is ready but has a hold -- leaving as is\n",
                        strand);
                strand->properties &= ~PDST_LOCK;
            }
        } else {
            strand->properties &= ~PDST_LOCK;
        }

        // We still hold lock on curNode EXCEPT if didFree
        if (propagateReady || propagateNP) {
            ASSERT(!didFree);
            DPRINTF(DEBUG_LVL_VERB, "Propagating properties: ready: %"PRIu32"; np: %"PRIu32"\n",
                    propagateReady, propagateNP);

            pdStrandTableNode_t *parent = curNode->parent;
            ASSERT(curNode->lock == 1);
            // We flipped nodeReady from 0 to 1; to up until we see a 1
            // We flipped nodeNeedsProcessing from 0 to 1; same as above
            PROPAGATE_UP_TREE(
                curNode, parent,
                propagateReady || propagateNP, {
                    if (propagateReady) {
                        propagateReady = parent->nodeReady == 0ULL;
                        parent->nodeReady |= (1ULL<<curNode->parentSlot);
                    }
                    if (propagateNP) {
                        propagateNP = parent->nodeNeedsProcess == 0ULL;
                        parent->nodeNeedsProcess |= (1ULL<<curNode->parentSlot);
                    }
                });
        } else {
            if(!didFree)
                hal_unlock32(&(curNode->lock));
        }
    }
END_LABEL(markReadyEventEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdMarkReadyEvent -> %"PRIu32"\n",
            toReturn);
    return toReturn;
#undef _END_FUNC
}

u8 pdMarkWaitEvent(ocrPolicyDomain_t *pd, pdEvent_t *evt) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdMarkWaitEvent(pd:%p, evt:%p)\n",
            pd, evt);
#define _END_FUNC markWaitEventEnd

    u8 toReturn = 0;
    if(pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    CHECK_RESULT_T(evt != NULL, , toReturn = OCR_EINVAL);
    CHECK_RESULT_T((evt->properties & PDEVT_READY) == PDEVT_READY, , toReturn = OCR_EINVAL);

    evt->properties &= ~PDEVT_READY;
    if(evt->strand != NULL) {
        DPRINTF(DEBUG_LVL_VERB, "Event has strand %p -> going to update\n", evt->strand);
        // First grab the lock on the strand
        pdStrand_t *strand = evt->strand;
        u32 stIdx = strand->index & ((1<<BV_SIZE_LOG2) - 1);
        pdStrandTableNode_t *curNode = strand->parent;
        ASSERT(curNode);

        // Lock the strand
        RESULT_ASSERT(_pdLockStrand(strand, 0, BLOCK), ==, 0);

        // This should be the case since the event was ready prior to this
        ASSERT((strand->properties & PDST_WAIT_EVT) == 0);
        strand->properties |= PDST_WAIT_EVT;

        // The following things can happen here
        //     - there are actions so this strand no longer needs processing
        //     - there are no actions -> this strand is no longer ready
        bool propagateReady = false, propagateNP = false;
        hal_lock32(&(curNode->lock));
        if ((strand->properties & PDST_WAIT_ACT) != 0) {
            DPRINTF(DEBUG_LVL_VERB, "(POSSIBLE RACE) Strand %p has waiting actions\n", strand);
            // We are no longer in need of processing
            curNode->nodeNeedsProcess &= ~(1ULL<<stIdx);
            propagateNP = curNode->nodeNeedsProcess == 0ULL;
        } else {
            DPRINTF(DEBUG_LVL_VERB, "Strand %p is no longer ready\n", strand);
            // If we are still around, it means we have an active hold
            ASSERT(strand->properties & PDST_HOLD);
            curNode->nodeReady &= ~(1ULL<<stIdx);
            propagateReady = curNode->nodeReady == 0ULL;
        }

        strand->properties &= ~PDST_LOCK;

        // We still hold lock on curNode
        if (propagateReady || propagateNP) {
            DPRINTF(DEBUG_LVL_VERB, "Propagating properties: ready: %"PRIu32"; np: %"PRIu32"\n",
                    propagateReady, propagateNP);

            pdStrandTableNode_t *parent = curNode->parent;
            ASSERT(curNode->lock == 1);
            // We flipped nodeReady from 1 to 0; to up until we see a sibbling
            // We flipped nodeNeedsProcessing from 1 to 0; same as above
            PROPAGATE_UP_TREE(
                curNode, parent,
                propagateReady || propagateNP, {
                    if (propagateReady) {
                        parent->nodeReady &= ~(1ULL<<curNode->parentSlot);
                        propagateReady = parent->nodeReady == 0ULL;
                    }
                    if (propagateNP) {
                        parent->nodeNeedsProcess &= ~(1ULL<<curNode->parentSlot);
                        propagateNP = parent->nodeNeedsProcess == 0ULL;
                    }
                });
        } else {
            hal_unlock32(&(curNode->lock));
        }
    }
END_LABEL(markWaitEventEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdMarkWaitEvent -> %"PRIu32"\n",
            toReturn);
    return toReturn;
#undef _END_FUNC
}

/***************************************/
/***** pdAction_t related functions ****/
/***************************************/

/* Macros defining the special encoding of pdAction_t* */
#define PDACTION_ENC_PROCESS_MESSAGE  0b001
#define PDACTION_ENC_EXTEND           0b111

pdAction_t* pdGetProcessMessageAction() {

    DPRINTF(DEBUG_LVL_INFO, "ENTER pdGetCallbackAction()\n");
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdGetCallbackAction -> action:0x%"PRIx64"\n", (u64)PDACTION_ENC_PROCESS_MESSAGE);
    return (pdAction_t*)(0x0ULL | PDACTION_ENC_PROCESS_MESSAGE);
}

/***************************************/
/***** pdStrand_t related functions ****/
/***************************************/

u8 pdInitializeStrandTable(ocrPolicyDomain_t* pd, pdStrandTable_t *table,
                           u32 properties) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdInitializeStrandTable(pd:%p, table:%p, props:0x%"PRIx32")\n",
            pd, table, properties);
#define _END_FUNC initializeStrandTableEnd
    u8 toReturn = 0;

    CHECK_RESULT_T(table != NULL, , toReturn |= OCR_EINVAL);

    table->levelCount = 0;
    table->head = NULL;
    table->lock = 0;

END_LABEL(initializeStrandTableEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdInitializeStrandTable -> %"PRIu32"\n",
            toReturn);
    return toReturn;
#undef _END_FUNC

}

u8 pdDestroyStrandTable(ocrPolicyDomain_t* pd, pdStrandTable_t *table,
                           u32 properties) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdDestroyStrandTable(pd:%p, table:%p, props:0x%"PRIx32")\n",
            pd, table, properties);
#define _END_FUNC destroyStrandTableEnd
    u8 toReturn = 0;

    CHECK_RESULT_T(table != NULL, , toReturn |= OCR_EINVAL);
    hal_lock32(&(table->lock));
    if (table->head != NULL) {
        // If the head exists, make sure all nodes are marked as free
        hal_lock32(&(table->head->lock));
        CHECK_RESULT_T(table->head->nodeFree == ~0ULL, {
                hal_unlock32(&(table->head->lock));
                hal_unlock32(&(table->lock));
            }, toReturn |= OCR_EINVAL);
        hal_unlock32(&(table->head->lock));
        // At this point, we hold the lock on the table and unless something is
        // fishy, since the table is empty, nothing else can be happening in
        // parallel. We will therefore free stuff happily.
        _pdDestroyStrandTableNode(pd, table->head);
        pd->fcts.pdFree(pd, table->head);
    } else {
        DPRINTF(DEBUG_LVL_VERB, "Freeing NULL table\n");
    }
    hal_unlock32(&(table->lock));

END_LABEL(destroyStrandTableEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdDestroyStrandTable -> %"PRIu32"\n",
            toReturn);
    return toReturn;
#undef _END_FUNC
}

// NOTE on the lock order: if you want to hold multiple locks, you must hold
// the lock for the child FIRST and then acquire the lock of your parent.

u8 pdGetNewStrand(ocrPolicyDomain_t *pd, pdStrand_t **returnStrand, pdStrandTable_t *table,
                  pdEvent_t* event, u32 properties) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdGetNewStrand(pd:%p, strand**:%p [%p], table:%p)\n",
            pd, returnStrand, *returnStrand, table);
#define _END_FUNC getNewStrandEnd

    u8 toReturn = 0;
    *returnStrand = NULL;
    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // We check if the properties are sane
    // This makes sure that no other bits than those allowed are set
    CHECK_RESULT(properties & ~(PDST_UHOLD), , toReturn |= OCR_EINVAL);

    pdStrandTableNode_t *leafToUse = NULL;
    hal_lock32(&(table->lock));
    // Look for a leaf to use
    u32 curLevel = 1;
    u32 cachedLevelCount = table->levelCount; // To be able to release the lock earlier
    if (table->levelCount == 0) {
        // If level is 0, it means that the table should be empty
        // We need to initialize it
        ASSERT(table->head == NULL);
        DPRINTF(DEBUG_LVL_VERB, "Table %p: empty -- adding level 1\n",
                table);
        // See BUG #899: this should be slab allocated
        CHECK_MALLOC(table->head = (pdStrandTableNode_t*)pd->fcts.pdMalloc
                     (pd, sizeof(pdStrandTableNode_t)), hal_unlock32(&(table->lock)));
        CHECK_RESULT(
            toReturn |= _pdInitializeStrandTableNode(pd, table->head, NULL, 0, 0, PDST_NODE_SIZE, IS_LEAF),
                     {hal_unlock32(&(table->lock)); pd->fcts.pdFree(pd, table->head);},);
        DPRINTF(DEBUG_LVL_VVERB, "Table %p: added head %p\n", table, table->head);
        table->levelCount = 1;
        cachedLevelCount = 1;
        leafToUse = table->head;
        hal_lock32(&(leafToUse->lock)); // Need lock to be able to read nodeFree later in a race-free manner
                                        // (otherwise, another thread may grab the head)
        hal_unlock32(&(table->lock));
        // Lock held here: leafToUse
    } else {
        // In this case, there is at least something in the table, go down the tree
        // to find something that is free. If nothing is found, we will create a new
        // leaf node
        pdStrandTableNode_t *curNode = table->head;
        hal_lock32(&(curNode->lock)); // Need lock to check curNode->nodeFree

        // Before releasing the table, we check if we have a snowball's chance in hell
        // of getting a free slot
        if(curNode->nodeFree == 0ULL) {
            // Nothing at all is free
            DPRINTF(DEBUG_LVL_VERB, "Table %p: fully loaded -- adding level %"PRIu32"\n",
                    table, cachedLevelCount + 1);
            pdStrandTableNode_t *newNode = NULL;
            // See BUG #899: this should be slab allocated
            CHECK_MALLOC(newNode = (pdStrandTableNode_t*)pd->fcts.pdMalloc
                         (pd, sizeof(pdStrandTableNode_t)),
                {hal_unlock32(&(curNode->lock)); hal_unlock32(&(table->lock));});

            // We don't initialize any sub nodes. This is also always a non-leaf node
            CHECK_RESULT(
                toReturn |=_pdInitializeStrandTableNode(pd, newNode, NULL, 0, cachedLevelCount, 0, 0),
                {pd->fcts.pdFree(pd, table->head); hal_unlock32(&(curNode->lock)); hal_unlock32(&(table->lock));},);

            // We need to "update" curNode to pretend we initialized it. In particular, we need
            // to set the parent
            curNode->parent = newNode;
            curNode->parentSlot = 0;
            RESULT_ASSERT(_pdSetStrandNodeAtIdx(pd, newNode, 0, curNode, 0), ==, 0);
            hal_unlock32(&(curNode->lock));

            // newNode->nodeFree should be 0 at bit 0 since curNode is full)
            ASSERT((newNode->nodeFree & 1ULL) == 0ULL);
            // Check the other bit vectors in a similar fashion
            ASSERT((newNode->nodeNeedsProcess & 1ULL) == (curNode->nodeNeedsProcess != 0ULL));
            ASSERT((newNode->nodeReady & 1ULL) == (curNode->nodeReady != 0ULL));

            DPRINTF(DEBUG_LVL_VVERB, "Table %p: level 1 is now %p (from %p)\n",
                    table, newNode, curNode);
            cachedLevelCount = ++table->levelCount;
            curNode = table->head = newNode;
            hal_lock32(&(curNode->lock)); // At this point, the head is visible
            hal_unlock32(&(table->lock));
            // Lock held here: curNode
        } else if(table->levelCount == 1) {
            DPRINTF(DEBUG_LVL_VERB, "Table %p has one level with free space (%p)\n",
                    table, curNode);
            hal_unlock32(&(table->lock));
            // If we have some free room and only one level, we know what to use
            leafToUse = curNode;
            // Lock held here: curNode/leafToUse

        } else {
            DPRINTF(DEBUG_LVL_VERB, "Proceeding down table with curNode %p\n",
                    curNode);
            hal_unlock32(&(table->lock));

        }

        // At this point, we hold the lock on curNode and leafToUse (if set)

        // Now we know that we at least have room in our tree to accomodate a new
        // strand.
        while(leafToUse == NULL) {
            ASSERT(curNode->nodeFree);  // We should never go to a place that has
                                        // no room
            ASSERT(curLevel < cachedLevelCount); // We never go all the way to the leaf
            u32 freeSlot = ctz(curNode->nodeFree);

            pdStrandTableNode_t **node = &(curNode->data.nodes[freeSlot]);
            DPRINTF(DEBUG_LVL_VERB, "Found free slot %"PRIu32" [%p] at level %"PRIu32" [%p]\n",
                    freeSlot, *node, curLevel, curNode);
            if (*node == NULL) {
                // See BUG #899: This should be slab allocated
                pdStrandTableNode_t *t = NULL;
                CHECK_MALLOC(
                    t = (pdStrandTableNode_t*)pd->fcts.pdMalloc(pd, sizeof(pdStrandTableNode_t)),
                                                                hal_unlock32(&(curNode->lock)));
                // If we are at the penultimate level, create a leaf node, otherwise
                // create a regular one
                if (curLevel == cachedLevelCount - 1) {
                    DPRINTF(DEBUG_LVL_VVERB, "Initializing leaf-node %p at level %"PRIu32"\n",
                            t, curLevel+1);
                    CHECK_RESULT(
                        toReturn |= _pdInitializeStrandTableNode(pd, t, curNode,
                                                                 freeSlot, 0,
                                                                 PDST_NODE_SIZE, IS_LEAF),
                                 {pd->fcts.pdFree(pd, *node); hal_unlock32(&(curNode->lock));},);
                    // An error here indicates a runtime logic error
                    RESULT_ASSERT(_pdSetStrandNodeAtIdx(pd, curNode, freeSlot, t, 0), ==, 0);
                    // Set the bit for nodeFree on curNode to prevent anyone else from
                    // going down this path.
                    curNode->nodeFree &= ~(1ULL<<freeSlot);
                    hal_unlock32(&(curNode->lock));
                    leafToUse = *node = t;
                    hal_lock32(&(leafToUse->lock));
                    // Lock held here: leafToUse
                } else {
                    CHECK_RESULT(
                        toReturn |= _pdInitializeStrandTableNode(pd, t, curNode,
                                                                 freeSlot, cachedLevelCount - curLevel,
                                                                 0, 0),
                        {pd->fcts.pdFree(pd, t); hal_unlock32(&(curNode->lock));},);
                    // An error here indicates a runtime logic error
                    RESULT_ASSERT(_pdSetStrandNodeAtIdx(pd, curNode, freeSlot, t, 0), ==, 0);
                    // Set the bit for nodeFree on curNode to prevent anyone else from
                    // going down this path.
                    curNode->nodeFree &= ~(1ULL<<freeSlot);
                    hal_unlock32(&(curNode->lock));
                    curNode = *node = t;
                    hal_lock32(&(curNode->lock));
                    // Lock held here: curNode
                }
            } else {
                // Set the bit for nodeFree on curNode to prevent anyone else from
                // going down this path.
                curNode->nodeFree &= ~(1ULL<<freeSlot);
                hal_unlock32(&(curNode->lock));
                // The node exists. If this is a leaf node, we are good to go
                if (curLevel == cachedLevelCount - 1) {
                    leafToUse = *node;
                    hal_lock32(&(leafToUse->lock));
                } else {
                    curNode = *node;
                    hal_lock32(&(curNode->lock));
                }
            }
            ++curLevel;
        }
    }

    // At this point:
    //  - we hold the lock on leafToUse (and nothing else)
    //  - we have "locked" down the path by saying there is no-one free
    //    in our path so no-one else should be trying to compete for our
    //    space.
    //  - there is room to add a strand
    // In this implementation, the leaf nodes are always fully initialized
    // so any child will exist. This can be changed by changing the parameters
    // to _pdInitializeStrandTableNode if needed (for example, only create all of
    // them for the first leaf node and then don't create any or just half).

    // We should have room in our leaf
    ASSERT(leafToUse->nodeFree);
    ASSERT(curLevel == cachedLevelCount); // We should be at the leaf level
    u32 freeSlot = ctz(leafToUse->nodeFree);

    pdStrand_t *strand = leafToUse->data.slots[freeSlot];
    DPRINTF(DEBUG_LVL_VERB, "Found free strand %"PRIu32" [%p] at leaf level %"PRIu32" [%p]\n",
            freeSlot, strand, curLevel, leafToUse);

    // All strands should be initialized here.
    ASSERT(strand);

    // The strand should be free
    RESULT_ASSERT(_pdLockStrand(strand, PDST_FREE, HAS_EXPECTED_VALUE | BLOCK), ==, 0);
    strand->curEvent = event;
    strand->actions = NULL;
    strand->properties |= PDST_RHOLD |
        (((event->properties & PDEVT_READY) != 0)?0:PDST_WAIT_EVT);
    strand->properties |= properties;
    DPRINTF(DEBUG_LVL_VVERB, "Strand %p: event: %p | actions: %p | props: 0x%"PRIx32"\n",
            strand, strand->curEvent, strand->actions, strand->properties);

    // Now set the value for the event
    event->strand = strand;
    *returnStrand = strand;

    // Now set the proper bits in the bit vectors and go up the chain
    // and update their bits if needed as well

    u8 propagateReady = false;
    leafToUse->nodeFree &= ~(1ULL<<freeSlot);

    // After inserting an event in a new strand, there
    // is definitely no way for the event to need processing
    // (which requires actions to process)

    // It can be ready though
    if((strand->properties & PDST_WAIT) == 0) {
        leafToUse->nodeReady |= (1ULL<<freeSlot);
        propagateReady = true;
    }

    pdStrandTableNode_t *curNode = leafToUse;
    pdStrandTableNode_t *parent = leafToUse->parent;
    ASSERT(curNode->lock  == 1);
    // This propagation needs to go all the way up irrespective
    // since we cleared the nodeFree bit to go down and know nothing of
    // sibling states
    PROPAGATE_UP_TREE(curNode, parent, true, {
            /* We had cleared the bit on the way down */
            ASSERT((parent->nodeFree & (1ULL<< curNode->parentSlot)) == 0);
            if (curNode->nodeFree) {
                parent->nodeFree |= (1ULL<<curNode->parentSlot);
            }

            if (propagateReady) {
                propagateReady = parent->nodeReady == 0ULL;
                parent->nodeReady |= (1ULL << curNode->parentSlot);
            }
        });
END_LABEL(getNewStrandEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdGetNewStrand -> %"PRIu32" [strand: %p]\n",
            toReturn, *returnStrand);
    return toReturn;
#undef _END_FUNC
}


u8 pdGetStrandForIndex(ocrPolicyDomain_t* pd, pdStrand_t **returnStrand, pdStrandTable_t* table,
                       u64 index) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdGetStrandForIndex(pd:%p, strand**:%p [%p], table:%p, idx:%"PRIu64")\n",
            pd, returnStrand, *returnStrand, table, index);
#define _END_FUNC getStrandForIndex

    u8 toReturn = 0;
    *returnStrand = NULL;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // We will just go down the tree until we find the strand we need
    hal_lock32(&(table->lock));
    u32 maxLevel = table->levelCount;
    pdStrandTableNode_t *curNode = table->head;
    u32 curIndex;
    hal_unlock32(&(table->lock));
    if (maxLevel == 0) {
        DPRINTF(DEBUG_LVL_WARN, "Table empty; index %"PRIu64" not found\n", index);
        CHECK_RESULT_T(false, , toReturn = OCR_EINVAL);
    }
    u32 curLevel = 1;
#define _LVL_MASK ((1ULL<<BV_SIZE_LOG2) - 1)
    if (index > (1ULL<<(maxLevel*BV_SIZE_LOG2))) {
        DPRINTF(DEBUG_LVL_WARN, "Table has only %"PRIu32" levels; cannot contain %"PRIu64"\n",
                maxLevel, index);
        CHECK_RESULT_T(false, , toReturn = OCR_EINVAL);
    }
    // At this point, we are pretty sure that the index is valid in the table
    while (curLevel < maxLevel) {
        curIndex = (index >> (BV_SIZE_LOG2*(maxLevel-curLevel))) & _LVL_MASK;
        curNode = curNode->data.nodes[curIndex];
        ++curLevel;
    }
    // Now extract the strand at the last level
    *returnStrand = curNode->data.slots[index & _LVL_MASK];

    // This makes sure that the strand actually has the proper index
    ASSERT((*returnStrand)->index == index);

END_LABEL(getStrandForIndex)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdStrandForIndex -> %"PRIu32" [strand: %p]\n",
            toReturn, *returnStrand);
    return toReturn;
#undef _LVL_MASK
#undef _END_FUNC
}


u8 pdEnqueueActions(ocrPolicyDomain_t *pd, pdStrand_t* strand, u32 actionCount,
                    pdAction_t** actions, u8 clearFwdHold) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdEnqueueActions(pd:%p, strand:%p, count:%"PRIu32", actions**:%p [%p], clearHold:%"PRIu32")\n",
            pd, strand, actionCount, actions, *actions, clearFwdHold);
#define _END_FUNC enqueueActionsEnd

    u8 toReturn = 0;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // Basic sanity checks
    // !(actionCount == 0 || actions != NULL)
    CHECK_RESULT_T(actionCount == 0 || actions != NULL, , toReturn = OCR_EINVAL);

    // A lock should be held while we enqueue actions. Make sure it is. If this
    // fails, most likely an internal runtime error
    ASSERT(strand->properties & PDST_LOCK);

    // Create the deque if it doesn't exist
    if (strand->actions == NULL) {
        // We only need a non-locked queue since we are taking care of the lock
        // ourself
        CHECK_MALLOC(strand->actions = newDeque(pd, NULL, NON_CONCURRENT_DEQUE), );
    }

    DPRINTF(DEBUG_LVL_VERB, "Going to enqueue %"PRIu32" actions on %p\n",
            actionCount, strand->actions);
    // At this point, we can enqueue things on the strand->actions deque
    u32 i;
    for (i = 0; i < actionCount; ++i, ++actions) {
        DPRINTF(DEBUG_LVL_VVERB, "Pushing action %p\n", *actions);
        strand->actions->pushAtTail(strand->actions, *actions, 1);
    }

    if (strand->actions->size(strand->actions) == actionCount) {
        // This means that no actions were pending
        DPRINTF(DEBUG_LVL_VVERB, "Strand %p had no actions [props: 0x%"PRIx32"] -> setting WAIT_ACT\n",
                strand, strand->properties);
        ASSERT((strand->properties & PDST_WAIT_ACT) == 0);
        strand->properties |= PDST_WAIT_ACT;
        DPRINTF(DEBUG_LVL_VVERB, "Strand %p [props: 0x%"PRIx32"]\n", strand,
                strand->properties);

        if((strand->properties & PDST_WAIT_EVT) == 0) {
            // Check if the event was also ready. If that's
            // the case, we need to switch to being *not* ready
            // We need to propagate that back up the tree
            pdStrandTableNode_t *curNode = strand->parent;
            ASSERT(curNode);
            pdStrandTableNode_t *parent = curNode->parent;
            u32 stIdx = strand->index & ((1<<BV_SIZE_LOG2)-1);
            bool propagateReady = false, propagateNP = false;

            hal_lock32(&(curNode->lock));
            // We need processing
            propagateNP = curNode->nodeNeedsProcess == 0ULL;
            curNode->nodeNeedsProcess |= (1ULL<<stIdx);
            // We are no longer ready
            ASSERT((curNode->nodeReady & (1ULL<<stIdx)) != 0);
            curNode->nodeReady &= ~(1ULL<<stIdx);
            propagateReady = (curNode->nodeReady == 0ULL);

            ASSERT(curNode->lock  == 1);
            // In this case, we flipped:
            // NP from 0 to 1 (stop when we see a 1)
            // Ready from 1 to 0 (stop when sibblings have ready nodes)
            PROPAGATE_UP_TREE(
                curNode, parent,
                propagateReady || propagateNP, {
                    if (propagateReady) {
                        parent->nodeReady &= ~(1ULL<<curNode->parentSlot);
                        propagateReady = parent->nodeReady == 0ULL;
                    }
                    if (propagateNP) {
                        propagateNP = parent->nodeNeedsProcess == 0ULL;
                        parent->nodeNeedsProcess |= (1ULL<<curNode->parentSlot);
                    }
                });
        }
    }
    if (clearFwdHold) {
        DPRINTF(DEBUG_LVL_VERB, "Clearing fwd hold on %p [props: 0x%"PRIx32"]\n",
                strand, strand->properties);
        if ((strand->properties & PDST_RHOLD) == 0) {
            DPRINTF(DEBUG_LVL_WARN, "Clearing non-existant hold on %p [props: 0x%"PRIx32"]\n",
                    strand, strand->properties);
        }
        strand->properties &= ~PDST_RHOLD;
    }
END_LABEL(enqueueActionsEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdEnqueueActions -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}


u8 pdLockStrand(pdStrand_t *strand, bool doTry) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdLockStrand(strand:%p, doTry:%"PRIu32")\n",
            strand, doTry);
#define _END_FUNC lockStrandEnd

    u8 toReturn = 0;
    toReturn = _pdLockStrand(strand, 0, doTry?0:BLOCK);

END_LABEL(lockStrandEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdLockStrand -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}

u8 pdUnlockStrand(pdStrand_t *strand) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdUnlockStrand(strand:%p)\n",
            strand);
#define _END_FUNC unlockStrandEnd

    u8 toReturn = 0;
    CHECK_RESULT_T(((strand->properties & PDST_LOCK)), , toReturn = OCR_EINVAL);

    strand->properties &= ~PDST_LOCK;

END_LABEL(unlockStrandEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdUnlockStrand -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}


/***************************************/
/****** Global scheduler functions *****/
/***************************************/


u8 pdProcessStrands(ocrPolicyDomain_t *pd, u32 properties) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER pdProcessStrands(pd:%p, props:0x%"PRIx32")\n",
            pd, properties);
#define _END_FUNC processStrandsEnd

    u8 toReturn = 0;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    /* In this function, we are currently very dumb and follow a simple algorithm.
     * In the future, this could be extended to having a plug-in model to write
     * what is basically a very fast scheduler for micro-tasks. I don't want to have
     * a full-fledged module model here because this part is performance sensitive
     * and I also do not think there are that many possibilities for such a "simple"
     * scheduler.
     * Ideas considered for the future:
     *    - some sort of callback to determine priorities
     *    - a better determination of how much work to do in one of these calls
     *    - dynamic throttling (related to previous point) or priorities
     *
     * Current algorithm:
     *    - look at all tables in the policy domain and for each table do the following:
     *    - look for strands that require action processing up to X (compile time
     *      constant) times.
     *    - for all such strands, process their action queue until stuck again
     */


    u32 i = 0;
    u32 processCount = 0;
    u32 curLevel = 1;
    for (; i < PDSTT_COMM; ++i) {
        pdStrandTable_t *table = pd->strandTables[i];
        DPRINTF(DEBUG_LVL_VERB, "Looking at table %p [idx: %"PRIu32"]\n", table, i);
        processCount = 0;
        hal_lock32(&(table->lock));
        pdStrandTableNode_t *curNode = table->head;
        hal_unlock32(&(table->lock));
        if (curNode == NULL) {
            DPRINTF(DEBUG_LVL_VVERB, "Table empty -- continuing to next table\n");
            continue; // We don't even have a head so we really don't have much to do
        }
        hal_lock32(&(curNode->lock));
        // Continue "forever" if emptytables or just until the maximum count is reached
        while ((properties & PDSTT_EMPTYTABLES) || (processCount < PDPROCESS_MAX_COUNT)) {
            // Go down the tree and see if we have nodeNeedsProcess set anywhere
            // Note that if multiple threads show up, the lock will serialize them
            // and they will each pick a different path ensuring at most 64 way parallelism
            // in this endeavor. This can also happen concurrently with adding new strands and
            // what not
            ASSERT(curNode);
            ASSERT(curNode->lock == 1);
            if (curNode->nodeNeedsProcess) {
                DPRINTF(DEBUG_LVL_VERB, "Node %p has children to process [0x%"PRIx64"]\n",
                        curNode, curNode->nodeNeedsProcess);
                u32 processSlot = ctz(curNode->nodeNeedsProcess);
                // Clear the bit for nodeNeedsProcess on curNode to prevent anyone else from
                // going down this path.
                curNode->nodeNeedsProcess &= ~(1ULL<<processSlot);

                if (!(curNode->lmIndex & 0x1)) {
                    // This is not a leaf node so we need to keep going down
                    pdStrandTableNode_t *node = curNode->data.nodes[processSlot];
                    // If we have something that needs to be processed, there should definitely be a node
                    hal_unlock32(&(curNode->lock));
                    ASSERT(node);
                    DPRINTF(DEBUG_LVL_VERB, "Going down slot %"PRIu32" to %p\n", processSlot, node);
                    curNode = node;
                    hal_lock32(&(curNode->lock));
                    ++curLevel;
                    continue;
                }
                // If we are here, we are in a leaf node so we may have found something
                // to process

                ASSERT((curNode->nodeNeedsProcess & (1ULL<<processSlot)) == 0);
                pdStrand_t *toProcess = curNode->data.slots[processSlot];
                DPRINTF(DEBUG_LVL_VERB, "Found strand %p in slot %"PRIu32"\n", toProcess, processSlot);

                // Grab the lock on it and then we are going to release all the other "locks"
                // to free-up parallelism if needed.
                RESULT_ASSERT(_pdLockStrand(toProcess, 0, BLOCK), ==, 0);

                // At this point, we need to re-jigger the nodeNeedsProcess bits
                // back up to the top of the chain. We need to re-jigger all the
                // way up
                DPRINTF(DEBUG_LVL_VVERB, "Going back up the stack to set nodeNeedsProcess\n");
                pdStrandTableNode_t *parent = curNode->parent;
                ASSERT(curNode->lock == 1);
                PROPAGATE_UP_TREE(curNode, parent, true, {
                        /* We cleared it on the way down */
                        ASSERT((parent->nodeNeedsProcess & (1ULL<<curNode->parentSlot)) == 0);
                        if(curNode->nodeNeedsProcess != 0ULL) {
                            parent->nodeNeedsProcess |= (1ULL << curNode->parentSlot);
                        }
                    });

                curNode = toProcess->parent; // Reset properly

                // At this point, we found the strand to process so we can actually
                // go and process each action. We hold no locks except on the strand

                // First some sanity checks
                ASSERT((toProcess->properties & PDST_WAIT) == PDST_WAIT_ACT); // The node should have the event ready and stuff to process
                // We loop while the event is ready and there is stuff to do
                // Note that the actions may make the event not ready thus the importance
                // of checking every time
                while (((toProcess->properties & PDST_WAIT_EVT) == 0) &&
                       toProcess->actions->size(toProcess->actions)) {
                    pdAction_t *curAction = (pdAction_t*)(toProcess->actions->popFromHead(toProcess->actions, false));
                    ASSERT(curAction);
                    DPRINTF(DEBUG_LVL_VERB, "Processing action %p\n", curAction);
                    RESULT_ASSERT(_pdProcessAction(pd, toProcess, curAction, 0), ==, 0);
                    DPRINTF(DEBUG_LVL_VERB, "Done processing action %p\n", curAction);
                }

                // Update properties
                hal_lock32(&(curNode->lock));
                bool propagateReady = false, propagateNP = false, didFree = false;
                if(toProcess->actions->size(toProcess->actions) == 0) {
                    toProcess->properties &= ~(PDST_WAIT_ACT);
                    if((toProcess->properties & PDST_WAIT_EVT) == 0) {
                        DPRINTF(DEBUG_LVL_VERB, "Strand %p now ready\n", toProcess);
                        propagateReady = curNode->nodeReady == 0ULL;
                        ASSERT((curNode->nodeReady & (1ULL<<processSlot)) == 0);
                        ASSERT((curNode->nodeNeedsProcess & (1ULL<<processSlot)) == 0);

                        curNode->nodeReady |= (1ULL<<processSlot);
                    } else {
                        DPRINTF(DEBUG_LVL_VERB, "Strand %p is not ready and has no actions\n",
                                toProcess);
                        ASSERT((curNode->nodeReady & (1ULL<<processSlot)) == 0);
                        ASSERT((curNode->nodeNeedsProcess & (1ULL<<processSlot)) == 0);
                        ASSERT((curNode->nodeReady & (1ULL<<processSlot)) == 0);
                    }
                } else {
                    ASSERT(toProcess->properties & PDST_WAIT_ACT);
                    if((toProcess->properties & PDST_WAIT_EVT) == 0) {
                        DPRINTF(DEBUG_LVL_VERB, "Strand %p still has pending actions that need processing\n",
                                toProcess);
                        propagateNP = curNode->nodeNeedsProcess == 0ULL;
                        curNode->nodeNeedsProcess |= (1ULL<<processSlot);

                        ASSERT((curNode->nodeReady & (1ULL<<processSlot)) == 0);
                    } else {
                        DPRINTF(DEBUG_LVL_VERB, "Strand %p has pending actions but not ready\n",
                                toProcess);

                        ASSERT((curNode->nodeNeedsProcess & (1ULL<<processSlot)) == 0);
                        ASSERT((curNode->nodeReady & (1ULL<<processSlot)) == 0);
                    }
                }

                // Holding curNode->lock and strand lock

                // Do clean up actions:
                if ((toProcess->properties & PDST_WAIT) == 0) {
                    // Strand is ready; we either free it or keep it there due to a hold
                    if ((toProcess->properties & PDST_HOLD) == 0) {
                        // We can free the strand now
                        DPRINTF(DEBUG_LVL_VERB, "Freeing strand %p [idx %"PRIu64"] after processing actions\n",
                                toProcess, toProcess->index);
                        // We unset the nodeReady bit to prevent unecessary propagation
                        curNode->nodeReady &= ~(1ULL<<processSlot);
                        hal_unlock32(&(curNode->lock));
                        RESULT_ASSERT(_pdFreeStrand(pd, toProcess), ==, 0);
                        ASSERT(!propagateNP);
                        propagateReady = false; // No need to change this since we freed the node
                        didFree = true;
                    } else {
                        DPRINTF(DEBUG_LVL_VERB, "Strand %p is ready but has a hold -- leaving as is\n",
                                toProcess);
                        toProcess->properties &= ~PDST_LOCK;
                    }
                } else {
                    toProcess->properties &= ~PDST_LOCK;
                }

                // Holding lock on curNode->lock EXCEPT if freed strand
                // (in that case, the following if statement is false)
                if (propagateReady || propagateNP) {
                    ASSERT(!didFree);
                    DPRINTF(DEBUG_LVL_VERB, "Propagating properties: ready: %"PRIu32"; np: %"PRIu32"\n",
                            propagateReady, propagateNP);

                    pdStrandTableNode_t *parent = curNode->parent;
                    ASSERT(curNode->lock == 1);
                    // We flipped nodeReady from 0 to 1; to up until we see a 1
                    // We flipped nodeNeedsProcessing from 0 to 1; same as above
                    PROPAGATE_UP_TREE(
                        curNode, parent,
                        propagateReady || propagateNP, {
                            if (propagateReady) {
                                propagateReady = parent->nodeReady == 0ULL;
                                parent->nodeReady |= (1ULL<<curNode->parentSlot);
                            }
                            if (propagateNP) {
                                propagateNP = parent->nodeNeedsProcess == 0ULL;
                                parent->nodeNeedsProcess |= (1ULL<<curNode->parentSlot);
                            }
                        });
                } else {
                    if(!didFree)
                        hal_unlock32(&(curNode->lock));
                }

                // We processed a node so we go and look for the next one
                ++processCount;
                hal_lock32(&(table->lock));
                curNode = table->head;
                hal_unlock32(&(table->lock));
                ASSERT(curNode); // The table can't empty out from under us
                hal_lock32(&(curNode->lock));
            } else {
                // Nothing left to process
                DPRINTF(DEBUG_LVL_VERB, "No actions to process -- breaking out after %"PRIu32"\n",
                        processCount);
                break; // Breaks out of processing loop
            }
        } /* End of while loop in one table */
        // At the end of the loop, we always hold the lock on curNode so
        // we release it
        hal_unlock32(&(curNode->lock));
    } /* End of for loop on tables */

END_LABEL(processStrandsEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT pdProcessStrands -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}


/***************************************/
/********** Internal functions *********/
/***************************************/

typedef pdEvent_t* (*actionCallback_t)(ocrPolicyDomain_t*, pdEvent_t*, u32);

static u8 _pdProcessAction(ocrPolicyDomain_t *pd, pdStrand_t *strand, pdAction_t* action,
                           u32 properties) {

    DPRINTF(DEBUG_LVL_INFO, "ENTER _pdProcessAction(pd:%p, strand:%p, action:%p, props:0x%"PRIx32")\n",
            pd, strand, action, properties);
#define _END_FUNC processActionEnd

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // If we are processing, the event better be ready
    ASSERT(strand->curEvent->properties & PDEVT_READY);

    u8 toReturn = 0;

    u64 actionPtr = (u64)action;

    // Figure out the encoding for the action
    switch (actionPtr & 0x7) {
        case PDACTION_ENC_PROCESS_MESSAGE:
        {
            actionCallback_t callback = (actionCallback_t)(pd->fcts.processMessageMT);
            DPRINTF(DEBUG_LVL_VERB, "Action is a callback to processMessage (%p)\n", callback);
            pdEvent_t* returnedValue = callback(pd, strand->curEvent, 0);
            if (returnedValue) {
                DPRINTF(DEBUG_LVL_VVERB, "Callback returned event %p -- replacing strand event %p\n",
                        returnedValue, strand->curEvent);
                /* SEE BUG #899: This should be a slab free */
                pd->fcts.pdFree(pd, strand->curEvent);
                strand->curEvent = returnedValue;
            } else {
                DPRINTF(DEBUG_LVL_VVERB, "Callback returned NULL\n");
            }
            break;
        }
        case PDACTION_ENC_EXTEND:
            ASSERT(0);
            break;
        default:
            /* For now, empty function, we just print something */
            DPRINTF(DEBUG_LVL_INFO, "Pretending to execute action %p\n", action);
            break;
    }


END_LABEL(processActionEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT _pdProcessAction -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}

static u8 _pdDestroyStrandTableNode(ocrPolicyDomain_t *pd, pdStrandTableNode_t *node) {

    DPRINTF(DEBUG_LVL_INFO, "ENTER _pdDestroyStrandTableNode(pd:%p, node:%p)\n",
            pd, node);
#define _END_FUNC destroyStrandTableNodeEnd

    u8 toReturn = 0;
    u32 i;

    CHECK_RESULT_T(node != NULL, , toReturn |= OCR_EINVAL);

    // This should not contain anything
    ASSERT(node->nodeFree == ~0ULL);
    bool isLeaf = node->lmIndex & 0x1;

    for (i=0; i<BV_SIZE; ++i) {
        if(node->data.slots[i] != NULL) {
            if(isLeaf) {
                DPRINTF(DEBUG_LVL_VERB, "Freeing strand %"PRIu32": %p\n",
                        i, node->data.slots[i]);
                pd->fcts.pdFree(pd, node->data.slots[i]);
                node->data.slots[i] = NULL;
            } else {
                DPRINTF(DEBUG_LVL_VERB, "Freeing down sub-node %"PRIu32": %p\n",
                        i, node->data.nodes[i]);
                CHECK_RESULT(toReturn |= _pdDestroyStrandTableNode(pd, node->data.nodes[i]), ,);
                DPRINTF(DEBUG_LVL_VERB, "Freeing su-node %"PRIu32": %p\n",
                        i, node->data.nodes[i]);
                pd->fcts.pdFree(pd, node->data.nodes[i]);
                node->data.nodes[i] = NULL;
            }
        }
    }

END_LABEL(destroyStrandTableNodeEnd)
        DPRINTF(DEBUG_LVL_INFO, "EXIT _pdDestroyStrandTableNode -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}

static u8 _pdLockStrand(pdStrand_t *strand, u32 expectedValue, u32 properties) {
    volatile u32 initialValue;
    if (properties & HAS_EXPECTED_VALUE) {
        initialValue = expectedValue;
        if ((initialValue & PDST_LOCK) == PDST_LOCK) {
            // The expected value is already locked, this is an error however you look
            // at it
            return ((properties & BLOCK) == BLOCK)?OCR_EINVAL:OCR_EBUSY;
        }
    } else {
        initialValue = strand->properties;
        if(((properties & BLOCK) == 0) && ((initialValue & PDST_LOCK) != 0)) {
            return OCR_EBUSY; // Already locked so no point trying
        }
    }

    // Try once
    u32 t, oldValue = initialValue;
    while ((oldValue & PDST_LOCK) == PDST_LOCK) {
        // We re-read the value
        // Note that in this case, we never have HAS_EXPECTED_VALUE set
        ASSERT((properties & HAS_EXPECTED_VALUE) == 0);
        oldValue = strand->properties;
    }
    t = oldValue;
    ASSERT((t & PDST_LOCK) == 0);
    oldValue = hal_cmpswap32(&(strand->properties), t, t | PDST_LOCK);

    if ((oldValue != t) && ((properties & BLOCK) == 0))
        return OCR_EBUSY;
    if (t == oldValue)
        return 0; // Successful

    // Go in a tighter loop now
    do {
        while ((oldValue & PDST_LOCK) == PDST_LOCK) {
            oldValue = strand->properties;
        }
        t = oldValue;
        ASSERT((t & PDST_LOCK) == 0);
        oldValue = hal_cmpswap32(&(strand->properties), t, t | PDST_LOCK);
    } while (t != oldValue);

    if (((properties & HAS_EXPECTED_VALUE) == HAS_EXPECTED_VALUE) && (initialValue != oldValue)) {
        strand->properties &= ~PDST_LOCK;
        return OCR_EINVAL;
    }
    return 0;
}


static u8 _pdInitializeStrandTableNode(ocrPolicyDomain_t *pd, pdStrandTableNode_t *node,
                                       pdStrandTableNode_t *parent, u32 parentSlot,
                                       u32 rdepth, u32 numChildrenToInit,
                                       u8 flags) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER _pdInitializeStrandTableNode(pd:%p, node:%p, parent:%p, pSlot:%"PRIu32", rdepth:%"PRIu32", childCount:%"PRIu32", flags:0x%"PRIx32")\n",
            pd, node, parent, parentSlot, rdepth, numChildrenToInit, flags);
#define _END_FUNC initializeStrandTableNodeEnd

    // Does not check for lock on node because could be in exclusive access
    // Does check on parent though if non-NULL
    u8 toReturn = 0;
    u32 i = 0;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    if (parent) {
        CHECK_RESULT_T(parent->lock, , toReturn = OCR_EINVAL);
    }

    // Some sanity checks
    ASSERT(node);
    ASSERT((parent == NULL) || parentSlot < BV_SIZE);
    ASSERT(numChildrenToInit <= BV_SIZE);
    if (!(flags & IS_LEAF)) {
        // If not a leaf node, numChildrenToInit should be 0
        CHECK_RESULT_T(numChildrenToInit == 0, , toReturn = OCR_EINVAL);
    } else {
        CHECK_RESULT(rdepth, , toReturn = OCR_EINVAL);
    }

    node->nodeFree = (u64)-1;   // All nodes are free to start with
    node->nodeNeedsProcess = 0; // Nothing needs to be processed
    node->nodeReady = 0;        // Nothing is ready
    // This is parent->lmIndex + parentSlot*BV_SIZE^(rdepth+1)
    node->lmIndex = parent?(parent->lmIndex + (parentSlot << (BV_SIZE_LOG2*(rdepth+1))))<<1:0;
    node->lock = 0;             // No lock for now.
    node->parent = parent;
    node->parentSlot = parent?parentSlot:(u32)-1; // If no parent, put -1

    if (flags & IS_LEAF) {
        // Indicate leaf status
        node->lmIndex |= 0x1;

        // Now take care of the data
        pdStrand_t * slab = NULL;
        if (numChildrenToInit) {
            // BUG #899: should be slab allocated
            // We allocate once so that if it fails, the cleanup is easy :)
            CHECK_MALLOC(slab = (pdStrand_t*)pd->fcts.pdMalloc(pd, sizeof(pdStrand_t)*numChildrenToInit),);
            DPRINTF(DEBUG_LVL_VERB, "Allocated %"PRIu32" strands for node %p\n",
                    numChildrenToInit, node);
        }
        // If we reach here, we allocated OK
        for (i = 0; i < numChildrenToInit; ++i, ++slab) {
            slab->curEvent = NULL;
            slab->actions = NULL;
            slab->parent = node;
            slab->properties = PDST_FREE;
            slab->index = (node->lmIndex>>1) + (u64)i;
            node->data.slots[i] = slab;
            DPRINTF(DEBUG_LVL_VVERB, "Created strand %"PRIu64" @ %p\n",
                    slab->index, slab);
        }
    }

    // We NULL-ify everything else. Note that this works in all cases because
    // numChildrenToInit will be 0 if not a leaf
    // Continues the previous loop if isLeaf
    for ( ; i < BV_SIZE; ++i) {
        node->data.slots[i] = NULL; // Does not matter if we use slots or nodes
    }


END_LABEL(initializeStrandTableNodeEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT _pdInitializeStrandTableNode -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}


static u8 _pdSetStrandNodeAtIdx(ocrPolicyDomain_t *pd, pdStrandTableNode_t *parent,
                                u32 idx, void* child, u8 flags) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER _pdSetStrandNodeAtIdx(pd:%p, parent:%p, idx:%"PRIu32", child:%p, flags:0x%"PRIx32")\n",
            pd, parent, idx, child, flags);
#define _END_FUNC setStrandNodeAtIdxEnd

    // Does not check for lock on anything since we may have exclusive access
    u8 toReturn = 0;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // Sanity check
    ASSERT(idx < BV_SIZE);

    // If this fails, it means there is already a child there
    CHECK_RESULT_T(parent->data.slots[idx] == NULL, , toReturn |= OCR_EACCES);

    // If this assert fails, this means a runtime error happened and state is
    // no longer consistent
    ASSERT(parent->nodeFree & (1ULL<<idx));

    // If this fails, the child is invalid
    if (flags & IS_STRAND) {
        CHECK_RESULT_T(((pdStrand_t*)child)->parent == parent, , toReturn |= OCR_EINVAL);
        CHECK_RESULT_T(((pdStrand_t*)child)->index >= (parent->lmIndex>>1), , toReturn |= OCR_EINVAL);
        CHECK_RESULT_T(((pdStrand_t*)child)->index < (parent->lmIndex>>1) + BV_SIZE, , toReturn |= OCR_EINVAL);
    } else {
        CHECK_RESULT_T(((pdStrandTableNode_t*)child)->parent == parent, , toReturn |= OCR_EINVAL);
        CHECK_RESULT_T(((pdStrandTableNode_t*)child)->parentSlot == idx, , toReturn |= OCR_EINVAL);
    }

    pdStrandTableNode_t *curNode = NULL;
    bool propagateFree = false, propagateReady = false, propagateNP = false;
    if (flags & IS_STRAND) {
        parent->data.slots[idx] = child;
        pdStrand_t *strand = (pdStrand_t*)child;
        // Update the flags
        if (!(strand->properties & PDST_FREE)) {
            parent->nodeFree &= ~(1ULL<<idx);
            propagateFree = parent->nodeFree == 0ULL;
        }

        if ((strand->properties & PDST_WAIT) == 0) {
            propagateReady = parent->nodeReady == 0ULL;
            parent->nodeReady |= (1ULL<<idx);
        }

        if ((strand->properties & PDST_WAIT) == PDST_WAIT_ACT) {
            // This means that we only have to wait for actions
            propagateNP = parent->nodeNeedsProcess == 0ULL;
            parent->nodeNeedsProcess |= (1ULL<<idx);
        }
        curNode = parent;
    } else {
        parent->data.nodes[idx] = child;
        // Update the flags
        pdStrandTableNode_t *childNode = (pdStrandTableNode_t*)child;
        if (childNode->nodeFree == 0ULL) {
            parent->nodeFree &= ~(1ULL<<idx);
            propagateFree = parent->nodeFree == 0ULL;
        }

        if (childNode->nodeReady != 0ULL) {
            propagateReady = parent->nodeReady == 0ULL;
            parent->nodeReady |= 1ULL<<idx;
        }

        if (childNode->nodeNeedsProcess != 0ULL) {
            propagateNP = parent->nodeNeedsProcess == 0ULL;
            parent->nodeNeedsProcess |= 1ULL<<idx;
        }
        curNode = parent;
    }

    parent = curNode->parent;

    // We need to propagate things (possibly)
    // Free bits: we flipped from 1 to 0, see if this makes others 0 (stop when sibblings have free slots)
    // Ready bits: we flipped from 0 to 1, see if this makes others 1 (stop when see 1)
    // NP bits: we flipped from 0 to 1, see if this makes others 1 (stop when see 1)


    // If we have the lock on curNode, we should *not* release it.
    PROPAGATE_UP_TREE_NO_UNLOCK(
        curNode, parent,
        propagateFree || propagateReady || propagateNP, {
            if (propagateFree) {
                parent->nodeFree &= ~(1ULL<<curNode->parentSlot);
                propagateFree = parent->nodeFree == 0ULL;
            }
            if (propagateReady) {
                propagateReady = parent->nodeReady == 0ULL;
                parent->nodeReady |= (1ULL<<curNode->parentSlot);
            }
            if (propagateNP) {
                propagateNP = parent->nodeNeedsProcess == 0ULL;
                parent->nodeNeedsProcess |= (1ULL<<curNode->parentSlot);
            }
        });

END_LABEL(setStrandNodeAtIdxEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT _pdSetStrandNodeAtIdx -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}


static u8 _pdFreeStrand(ocrPolicyDomain_t* pd, pdStrand_t *strand) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER _pdFreeStrand(pd:%p, strand:%p)\n",
            pd, strand);
#define _END_FUNC freeStrandEnd

    u8 toReturn = 0;

    if (pd == NULL) {
        getCurrentEnv(&pd, NULL, NULL, NULL);
    }

    // The lock must be held on the strand
    CHECK_RESULT_T((strand->properties & PDST_LOCK), , toReturn = OCR_EINVAL);

    // We should not be freeing strands that are still going to be used, so a bit
    // of sanity check here
    CHECK_RESULT(strand->properties & PDST_WAIT, , toReturn = OCR_EINVAL);
    if (strand->actions) {
        CHECK_RESULT_T(strand->actions->size(strand->actions) != 0, , toReturn = OCR_EINVAL);
    }

    // At this stage, we can free the strand
    // Clean up data a bit:
    strand->curEvent = NULL;

    // Go up and hold the parent lock so we can propagate the proper information on
    // free slots
    pdStrandTableNode_t *curNode = strand->parent;
    ASSERT(curNode);
    hal_lock32(&(curNode->lock));

    // This should not fail since we have a lock. This is basically an unlock but
    // we make sure that no one else did something.
    RESULT_ASSERT(hal_cmpswap32(&(strand->properties), PDST_LOCK, PDST_FREE), ==, PDST_LOCK);

    // Propgate things up. We free a node and it may remove the
    // "ready" flag from it. It can't be NP because none of the
    // wait flags are set

    bool propagateReady = false, propagateFree = false;
    u32 stIdx = strand->index & ((1<<BV_SIZE_LOG2) - 1);

    ASSERT((curNode->nodeNeedsProcess & (1ULL<<stIdx)) == 0);

    // Only propagate if this is the first free slot we are adding
    propagateFree = curNode->nodeFree == 0ULL;
    curNode->nodeFree |= (1ULL<<stIdx);


    if(curNode->nodeReady & (1ULL<<stIdx)) {
        curNode->nodeReady &= ~(1ULL<<stIdx);
        // Only propagate if we removed the last ready node
        propagateReady = curNode->nodeReady == 0ULL;
    }
    pdStrandTableNode_t *parent = curNode->parent;

    ASSERT(curNode->lock == 1);
    // We flipped nodeFree from 0 to 1. Propagate until we hit a 1
    // We flipped nodeReady from 1 to 0. Propagate until we find sibblings
    PROPAGATE_UP_TREE(
        curNode, parent,
        propagateFree || propagateReady, {
            if (propagateFree) {
                propagateFree = parent->nodeFree == 0ULL;
                parent->nodeFree |= (1ULL<<curNode->parentSlot);
            }
            if (propagateReady) {
                parent->nodeReady &= ~(1ULL<<curNode->parentSlot);
                propagateReady = parent->nodeReady == 0ULL;
            }
        });
END_LABEL(freeStrandEnd)
    DPRINTF(DEBUG_LVL_INFO, "EXIT _pdFreeStrand -> %"PRIu32"\n", toReturn);
    return toReturn;
#undef _END_FUNC
}

