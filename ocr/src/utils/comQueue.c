/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#include "debug.h"

#include "ocr-errors.h"
#include "ocr-hal.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"

#include "utils/comQueue.h"

#define DEBUG_TYPE UTIL

u8 comQueueInit(comQueue_t *queue, u32 size, comQueueSlot_t *slots) {
    u32 i = 0;
    queue->readIdx = queue->writeIdx = 0;
    queue->size = size;
    queue->slots = slots;
    for(i=0; i<size; ++i) {
        queue->slots[i].status = COMQUEUE_WRITEABLE;
        initializePolicyMessage(&(queue->slots[i].msgBuffer), sizeof(ocrPolicyMsg_t));
        queue->slots[i].msgPtr = NULL;
    }
    return 0;
}

u8 comQueueReserveSlot(comQueue_t *queue, u32 *slot) {
    if(queue->size == 0) return OCR_ENOMEM;

    volatile u32 oldIdxValue;
    volatile u32 idxValue;
    u32 oldValue;
    if(queue->size == 1) {
        // For a queue with only one slot, we need to do things
        // a bit differently and just alternate between WRITEABLE and RESERVED
        if(queue->slots[0].status == COMQUEUE_WRITEABLE) {
            oldValue = hal_cmpswap32(&(queue->slots[0].status), COMQUEUE_WRITEABLE, COMQUEUE_RESERVED);
            if(oldValue == COMQUEUE_WRITEABLE) {
                *slot = 0;
                return 0;
            }
        }
        // All other cases: we could not get a slot
        return OCR_EAGAIN;
    }
    // This case is for size > 1
    ASSERT(queue->size > 1);

    do {
        oldIdxValue = queue->writeIdx;
        idxValue = (oldIdxValue + 1) % queue->size;
        hal_fence(); // We want to read the latest value
        if(queue->slots[idxValue].status == COMQUEUE_WRITEABLE) {
            // We can try jumping to that next slot
            oldValue = hal_cmpswap32(&(queue->writeIdx), oldIdxValue, idxValue);
            if(oldValue == oldIdxValue) {
                // We now have a potential hold on the slot BUT writeIdx could
                // loop all the way around and another thread could get here
                // before we set to COMQUEUE_RESERVED. We therefore need
                // to also atomically grab the slot
                oldValue = hal_cmpswap32(&(queue->slots[oldIdxValue].status),
                                         COMQUEUE_WRITEABLE, COMQUEUE_RESERVED);
                if(oldValue == COMQUEUE_WRITEABLE) {
                    // Definitely got the slot this time
                    *slot = oldIdxValue;
                    //DPRINTF(DEBUG_LVL_WARN, "0x%"PRIx32": G %"PRIu32"\n", queue, oldIdxValue);
                    return 0;
                } // else, we can just try again (no harm done, someone got the slot
            } // Else fall-through to try again
        } else {
            // We can't write to the queue
            return OCR_EAGAIN;
        }
    } while(true);
    ASSERT(0); // Should neve reach here
    return OCR_EAGAIN;
}

u8 comQueueUnreserveSlot(comQueue_t *queue, u32 slot) {
    // This is like validate but marks the slot empty
    ASSERT(slot < queue->size);
    ASSERT(queue->slots[slot].status == COMQUEUE_RESERVED);
    queue->slots[slot].status = queue->size==1?COMQUEUE_WRITEABLE:COMQUEUE_EMPTY;
    // No fence (it will propagate to reader lazily)
    return 0;
}

u8 comQueueValidateSlot(comQueue_t *queue, u32 slot) {
    ASSERT(slot < queue->size);
    ASSERT(queue->slots[slot].status == COMQUEUE_RESERVED);
    hal_fence(); // Make sure everything is actually written
    queue->slots[slot].status = COMQUEUE_FULL;
    // No fence (it will propage to the reader lazily)
    return 0;
}

u8 comQueueReadSlot(comQueue_t *queue, u32 *slot) {
    if(queue->size == 0) return OCR_ENOMEM;

    if(queue->size == 1) {
        // Special case for a queue with one slot
        if(queue->slots[0].status == COMQUEUE_FULL) {
            queue->slots[0].status = COMQUEUE_READING;
            *slot = 0;
            return 0;
        }
        return OCR_EAGAIN;
    }
    // Below works for queues with 2 slots or more
    ASSERT(queue->size > 1);
    // We start at readIdx and look for something that is COMQUEUE_FULL
    // We look up to writeIdx (including writeIdx)
    u32 lastIdx = queue->writeIdx; // We read now; it does not matter if we miss
                                   // some; we will catch them next time
    u32 firstIdx, curIdx, firstWriteable = (u32)-1, lastWriteable = 0;
    curIdx = queue->readIdx;
    firstIdx = curIdx;
    u8 allEmpty = true;

    // Now check for the rest of the queue.
    if(lastIdx < firstIdx) {
        while(curIdx < queue->size) {
            if(queue->slots[curIdx].status == COMQUEUE_FULL) {
                // We found a good slot
                queue->slots[curIdx].status = COMQUEUE_READING;
                // No fence needed here because writers don't care about this state

                // Only one reader, make sure no-one modified our readIdx
                ASSERT(queue->readIdx == firstIdx);
                if(firstWriteable != (u32)-1) {
                    // We had updates to readIdx so we fence to make sure all
                    // the status updates are visible and then we update readIdx
                    hal_fence();
                    //DPRINTF(DEBUG_LVL_WARN, "0x%"PRIx64": R: %"PRIu32" 3\n", queue, updatedReadIdx);
                    // We never wait on what we just set as writeable. Wait on the next
                    // "real" thing
                    queue->readIdx = (lastWriteable + 1) % queue->size;
                }
                *slot = curIdx;
                return 0;
            } else if(allEmpty && queue->slots[curIdx].status == COMQUEUE_EMPTY) {
                // Switch from EMPTY to WRITEABLE. This must be done only
                // by the reader thread
                queue->slots[curIdx].status = COMQUEUE_WRITEABLE;
                if(firstWriteable == (u32)-1) {
                    firstWriteable = curIdx;
                }
                lastWriteable = curIdx;
            } else {
                allEmpty = false;
            }
            ++curIdx;
        }
        // Restart in the loop below
        curIdx = 0;
    }
    while(curIdx <= lastIdx) {
        if(queue->slots[curIdx].status == COMQUEUE_FULL) {
            queue->slots[curIdx].status = COMQUEUE_READING;
            ASSERT(queue->readIdx == firstIdx);
            if(firstWriteable != (u32)-1) {
                // We had updates to readIdx so we fence to make sure all
                // the status updates are visible and then we update readIdx
                hal_fence();
                //DPRINTF(DEBUG_LVL_WARN, "0x%"PRIx64": R: %"PRIu32" 3\n", queue, updatedReadIdx);
                // We never wait on what we just set as writeable. Wait on the next
                // "real" thing
                queue->readIdx = (lastWriteable + 1) % queue->size;
            }
            *slot = curIdx;
            return 0;
        } else if(allEmpty && (queue->slots[curIdx].status == COMQUEUE_EMPTY)) {
            queue->slots[curIdx].status = COMQUEUE_WRITEABLE;
            if(firstWriteable == (u32)-1) {
                firstWriteable = curIdx;
            }
            lastWriteable = curIdx;
        } else {
            allEmpty = false;
        }
        ++curIdx;
    }
    ASSERT(queue->readIdx == firstIdx);
    if(firstWriteable != (u32)-1) {
        // We had updates to readIdx so we fence to make sure all
        // the status updates are visible and then we update readIdx
        hal_fence();
        //DPRINTF(DEBUG_LVL_WARN, "0x%"PRIx64": R: %"PRIu32" 3\n", queue, updatedReadIdx);
        // We never wait on what we just set as writeable. Wait on the next
        // "real" thing
        queue->readIdx = (lastWriteable + 1) % queue->size;
    }
    return OCR_EAGAIN;
}

u8 comQueueEmptySlot(comQueue_t *queue, u32 slot) {
    // The invariant maintained is all slots processed after readIdx (in a circular
    // manner) will be COMQUEUE_EMPTY. They switch to COMQUEUE_WRITEABLE
    // when readIdx is updated. There is a difference between COMQUEUE_EMPTY
    // and COMQUEUE_WRITEABLE because otherwise, the reserve could swap
    // writeIdx (incrementing it), this could read the updated value and
    // make readIdx "jump" over writeIdx
    ASSERT(slot < queue->size);
    ASSERT(queue->slots[slot].status == COMQUEUE_READING);

    u32 nextReadIdx;

    if(queue->size > 1 && slot == queue->readIdx) {
        queue->slots[slot].status = COMQUEUE_WRITEABLE;
        // If we were the read slot, advance. We advance by only 1, the poll
        // will take care of advancing more if needed
        nextReadIdx = (queue->readIdx + 1 ) % queue->size;
        hal_fence(); // Make sure all the updated status are visible.
    } else {
        queue->slots[slot].status = queue->size==1?COMQUEUE_WRITEABLE:COMQUEUE_EMPTY;
        // No fence needed, the writers don't really care about this one
        // We stay right where we are
        nextReadIdx = queue->readIdx;
    }

    //DPRINTF(DEBUG_LVL_WARN, "0x%"PRIx64": S: %"PRIu32" NR: %"PRIu32" 0\n", queue, slot, nextReadIdx);
    queue->readIdx = nextReadIdx;
    return 0;
}



