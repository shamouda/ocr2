/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_XE_PTHREAD

#include "debug.h"

#include "ocr-policy-domain.h"
#include "ocr-worker.h"
#include "ocr-comp-target.h"
#include "ocr-comp-platform.h"

#include "ocr-errors.h"
#include "ocr-hal.h"
#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"

#include "xe-pthread-comm-platform.h"
#include "../ce-pthread/ce-pthread-comm-platform.h"

#include "xstg-map.h"

#define DEBUG_TYPE COMM_PLATFORM

void xePthreadCommDestruct (ocrCommPlatform_t * base) {
    runtimeChunkFree((u64)base, PERSISTENT_CHUNK);
}


u8 xePthreadCommSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        if(properties & RL_BRING_UP) {
            if(RL_IS_LAST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // See explanations in ce-pthread-comm-platform.c for assumptions on this part
                ocrCommPlatformXePthread_t *rself = (ocrCommPlatformXePthread_t*)self;
                comQueueInit(&(rself->inQueue), 1,
                             (comQueueSlot_t*)runtimeChunkAlloc(sizeof(comQueueSlot_t), PERSISTENT_CHUNK));
                DPRINTF(DEBUG_LVL_VERB, "Initialized inQueue @ %p of size 1\n",
                        &(rself->inQueue));
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_CONFIG_PARSE, phase)) {
                // Free whatever we allocated during bring up
                ocrCommPlatformXePthread_t *rself = (ocrCommPlatformXePthread_t*)self;
                runtimeChunkFree((u64)(rself->inQueue.slots), PERSISTENT_CHUNK);
            }
        }
        break;
    case RL_NETWORK_OK:
        // Part in RL_PD_OK should be here
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_NETWORK_OK, phase)) {
            // This again relies on the fact that there is an implicit barrier between
            // RL_CONFIG_PARSE and this
            ocrCommPlatformXePthread_t * rself = (ocrCommPlatformXePthread_t*) self;
            ocrPolicyDomain_t * cePD = PD->parentPD;
            // We should have a parent
            ASSERT(cePD);
            ASSERT(PD->parentLocation == cePD->myLocation);
            ocrCommPlatformCePthread_t * parent =
                (ocrCommPlatformCePthread_t *) cePD->commApis[0]->commPlatform;
            rself->outQueue.neighborLocation = cePD->myLocation;
            rself->outQueue.outQueue = &(parent->inQueueXE);
            DPRINTF(DEBUG_LVL_VERB, "Outqueue for CE 0x%"PRIx64" is @ %p\n",
                    cePD->myLocation, &(parent->inQueueXE));
        }
        break;
    case RL_PD_OK:
        if ((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            //Initialize base
            self->pd = PD;
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }
    return toReturn;
}

/*
 * XE Non Blocking Send with one buffer space:
 * If buffer is empty send puts msg in it.
 * If buffer is not empty, return "busy" error code.
 */
u8 xePthreadCommSendMessage(ocrCommPlatform_t *self, ocrLocation_t target, ocrPolicyMsg_t *msg,
                            u64 *id, u32 properties, u32 mask) {
    ocrCommPlatformXePthread_t *rself = (ocrCommPlatformXePthread_t*)self;

    if(target != rself->outQueue.neighborLocation) {
        DPRINTF(DEBUG_LVL_WARN, "Destination for message @ %p to 0x%"PRIx64" not found\n",
                msg, target);
        ASSERT(0);
    }

    u8 status;
    u32 outSlot, inSlot;
    comQueue_t *queue = rself->outQueue.outQueue;
    comQueue_t *returnQueue = NULL;

    DPRINTF(DEBUG_LVL_VERB, "Sending msg %p (type: 0x%"PRIx32") to 0x%"PRIx64"; using out queue @ %p\n",
            msg, msg->type, target, queue);

    if((msg->type & (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) == (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) {
        // If this is a request that requires a response, we
        // will pre-reserve a slot on our incoming queue so that
        // the response can come back in. This is to avoid
        // deadlock situations (simple case: both CEs send to one another
        // simultaneously and then can't answer each other so they lock up
        returnQueue = &(rself->inQueue);
        status = comQueueReserveSlot(returnQueue, &inSlot);
        if(status == 0) {
            // We encode the queue address and slot to be able to make sure
            // things are OK on the way back
            msg->msgId = ((u64)returnQueue << 8) + inSlot;
            DPRINTF(DEBUG_LVL_VERB, "Message requires a response. Reserved answer slot %"PRIu32" on queue @ %p\n",
                    inSlot, returnQueue);
        } else if(status == OCR_ENOMEM) {
            // Permanent error
            DPRINTF(DEBUG_LVL_VVERB, "Empty queue! Permanent failure\n");
            return OCR_ENOMEM;
        } else {
            // Could not grab a slot
            DPRINTF(DEBUG_LVL_VVERB, "Message requires a response but local return queue @ %p busy\n",
                    returnQueue);
            return OCR_EBUSY;
        }
    }

    // If this is a response, we should already have a slot reserved for us
    if(msg->type & (PD_MSG_RESPONSE | PD_MSG_RESPONSE_OVERRIDE)) {
        ASSERT(msg->type & (PD_MSG_REQ_RESPONSE | PD_MSG_RESPONSE_OVERRIDE)); // We can't just be sending responses to no questions

        // Check if the queue matches (ie: we are sending back where we reserved)
        if(((u64)queue << 8) != (msg->msgId & 0xFFFFFFFFFFFFFF00ULL)) {
            DPRINTF(DEBUG_LVL_WARN, "Expected to send response to queue 0x%"PRIx64" but found queue %p\n",
                    msg->msgId >> 8, queue);
            ASSERT(0);
        }
        outSlot = (u32)(msg->msgId & 0xFFULL);
        status = 0;
        DPRINTF(DEBUG_LVL_VVERB, "Using pre-reserved slot %"PRIu32" on queue @ %p\n",
                outSlot, queue);
    } else {
        // Try to grab a slot on the destination queue to send
        status = comQueueReserveSlot(queue, &outSlot);
    }
    if(status == 0) {
        DPRINTF(DEBUG_LVL_VVERB, "Grabbed slot %"PRIu32" on queue @ %p to send to 0x%"PRIx64"\n",
                outSlot, queue, target);
        queue->slots[outSlot].properties = 0;
        // Check the properties flag to figure out if we need to copy the message
        if(properties & PERSIST_MSG_PROP) {
            // We just need to use the pointer
            DPRINTF(DEBUG_LVL_VVERB, "Sending pointer %p (no marshalling)\n", msg);
            queue->slots[outSlot].msgPtr = msg;
            if(!(properties & TWOWAY_MSG_PROP)) {
                // We need to free the pointer when we are done with it
                queue->slots[outSlot].properties |= COMQUEUE_FREE_PTR;
            }
        } else {
            DPRINTF(DEBUG_LVL_VERB, "Marshalling msg %p into %p\n",
                    msg, &(queue->slots[outSlot].msgBuffer));
            u64 baseSize = 0, marshalledSize = 0;
            ocrPolicyMsgGetMsgSize(msg, &baseSize, &marshalledSize, 0);
            // We do not support too large messages
            ASSERT(queue->slots[outSlot].msgBuffer.bufferSize >= baseSize + marshalledSize);
            ocrPolicyMsgMarshallMsg(msg, baseSize, (u8*)(&(queue->slots[outSlot].msgBuffer)),
                                    MARSHALL_FULL_COPY);
            queue->slots[outSlot].msgPtr = NULL; // Indicates that we need to use the msgBuffer
        }
        comQueueValidateSlot(queue, outSlot);
        if(id != NULL) {
            *id =  outSlot;
        }
    } else if(status == OCR_ENOMEM) {
        if(returnQueue) {
            // We need to invalidate the slot we reserved
            comQueueUnreserveSlot(returnQueue, inSlot);
        }
        // Permanent error
        DPRINTF(DEBUG_LVL_VVERB, "Empty queue! Permanent failure\n");
        return OCR_ENOMEM;
     } else {
        if(returnQueue) {
            // We need to invalidate the slot we reserved
            comQueueUnreserveSlot(returnQueue, inSlot);
        }
        // Could not grab a slot
        DPRINTF(DEBUG_LVL_VVERB, "Queue @ %p busy\n", queue);
        return OCR_EBUSY;
    }
    return 0;
}

u8 xePthreadCommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                            u32 properties, u32 *mask) {

    ocrCommPlatformXePthread_t *rself = (ocrCommPlatformXePthread_t*)self;
    // We look in our inQueue for a message
    u32 slot;
    u8 status;
    status = comQueueReadSlot(&(rself->inQueue), &slot);
    if(status == 0) {
        // We found a message
        u64 baseSize = 0, marshalledSize = 0;
        ocrPolicyMsg_t *curMsg = NULL;
        u8 fixupPtrs = 0;
        if(rself->inQueue.slots[slot].msgPtr == NULL) {
            // We have to use the buffer
            curMsg = &(rself->inQueue.slots[slot].msgBuffer);
            DPRINTF(DEBUG_LVL_VERB, "Found a message (in buffer) @ %p (type: 0x%"PRIx32")\n",
                    curMsg, curMsg->type);
            fixupPtrs = 1;
        } else {
            curMsg = rself->inQueue.slots[slot].msgPtr;
            DPRINTF(DEBUG_LVL_VERB, "Found a message as ptr @ %p (type: 0x%"PRIx32")\n",
                    curMsg, curMsg->type);
        }
        ocrPolicyMsgGetMsgSize(curMsg, &baseSize, &marshalledSize, 0);
        if(*msg && (*msg)->bufferSize >= baseSize + marshalledSize) {
            // We can copy things into here
            if(*msg != curMsg) {
                DPRINTF(DEBUG_LVL_VERB, "Going to copy message into hinted buffer (@ %p)\n",
                        *msg);
                ocrPolicyMsgUnMarshallMsg((u8*)curMsg, NULL, *msg, MARSHALL_FULL_COPY);
            } else {
                DPRINTF(DEBUG_LVL_VERB, "Hinted buffer is the same as the message\n");
                if(fixupPtrs) {
                    ocrPolicyMsgUnMarshallMsg((u8*)curMsg, NULL, *msg, MARSHALL_APPEND);
                }
            }
            // In both cases, we can empty the current slot as we have
            // a safe copy of the message
            // Free the message if we have to (persistent and not two ways)
            if(rself->inQueue.slots[slot].properties & COMQUEUE_FREE_PTR) {
                self->pd->fcts.pdFree(self->pd, curMsg);
            }
            DPRINTF(DEBUG_LVL_VERB, "Freeing slot %"PRIu32" of queue @ %p (at poll)\n",
                    slot, &(rself->inQueue));
            rself->inQueue.slots[slot].properties = 0;
            comQueueEmptySlot(&(rself->inQueue), slot);
        } else {
            // Can't use it
            // We return the pointer that we do have
            DPRINTF(DEBUG_LVL_VERB, "Not using hint... returning %p\n", curMsg);
            if(fixupPtrs) {
                ocrPolicyMsgUnMarshallMsg((u8*)curMsg, NULL, curMsg, MARSHALL_APPEND);
            }
            *msg = curMsg;
            // We mark the slot as something we need to free later
            // this is mostly for verification purposes
            // Also set msgPtr to always point to the message
            // so we don't have to look in two places
            rself->inQueue.slots[slot].msgPtr = curMsg;
            rself->inQueue.slots[slot].properties |= COMQUEUE_EMPTY_PENDING;
        }
        // We do not check if there are more messages (for now)
        return 0;
    } else if(status == OCR_ENOMEM) {
        return OCR_EINTR<<4; // This is not great because we can't encode full error codes
    } else {
        // This means there is no message yet
        return POLL_NO_MESSAGE;
    }
}

u8 xePthreadCommWaitMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                            u32 properties, u32 *mask) {
    while(xePthreadCommPollMessage(self, msg, properties, mask) != 0)
        ;
    return 0;
}

u8 xePthreadDestructMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t *msg) {
    // Here we need to see if we have a slot to free. We need to look
    // for COMQUEUE_EMPTY_PENDING
    ocrCommPlatformXePthread_t *rself = (ocrCommPlatformXePthread_t*)self;
    u32 i;
    for(i=0; i<rself->inQueue.size; ++i) {
        if((rself->inQueue.slots[i].properties & COMQUEUE_EMPTY_PENDING) &&
           (rself->inQueue.slots[i].msgPtr == msg)) {
            if(rself->inQueue.slots[i].properties & COMQUEUE_FREE_PTR) {
                self->pd->fcts.pdFree(self->pd, rself->inQueue.slots[i].msgPtr);
            }
            DPRINTF(DEBUG_LVL_VERB, "Freeing slot %"PRIu32" of queue @ %p\n",
                    i, &(rself->inQueue));
            rself->inQueue.slots[i].properties = 0;
            comQueueEmptySlot(&(rself->inQueue), i);
            return 0;
        }
    }
    DPRINTF(DEBUG_LVL_WARN, "Could not find message %p to destroy\n",
            msg);
    return OCR_EINVAL;
}

u64 xePthreadGetSeqIdAtNeighbor(ocrCommPlatform_t *self, ocrLocation_t neighborLoc, u64 neighborId) {
    ASSERT(0);
    return 0;
}

ocrCommPlatform_t* newCommPlatformXePthread(ocrCommPlatformFactory_t *factory,
        ocrParamList_t *perInstance) {

    ocrCommPlatformXePthread_t * commPlatformXePthread = (ocrCommPlatformXePthread_t*)
            runtimeChunkAlloc(sizeof(ocrCommPlatformXePthread_t), PERSISTENT_CHUNK);
    ocrCommPlatform_t * base = (ocrCommPlatform_t *) commPlatformXePthread;
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeCommPlatformXePthread(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * base, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, base, perInstance);
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCommPlatformFactoryXePthread(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrCommPlatformFactory_t *newCommPlatformFactoryXePthread(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryXePthread_t), PERSISTENT_CHUNK);

    base->instantiate = &newCommPlatformXePthread;
    base->initialize = &initializeCommPlatformXePthread;
    base->destruct = &destructCommPlatformFactoryXePthread;

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), xePthreadCommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                  phase_t, u32, void (*)(ocrPolicyDomain_t*,u64), u64), xePthreadCommSwitchRunlevel);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrLocation_t, ocrPolicyMsg_t *, u64*, u32, u32),
                                     xePthreadCommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t **, u32, u32*),
                                     xePthreadCommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t **, u32, u32*),
                                     xePthreadCommWaitMessage);
    base->platformFcts.destructMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t *),
                                                   xePthreadDestructMessage);
    base->platformFcts.getSeqIdAtNeighbor = FUNC_ADDR(u64 (*)(ocrCommPlatform_t*, ocrLocation_t, u64),
                                                   xePthreadGetSeqIdAtNeighbor);

    return base;
}
#endif /* ENABLE_COMM_PLATFORM_XE_PTHREAD */
