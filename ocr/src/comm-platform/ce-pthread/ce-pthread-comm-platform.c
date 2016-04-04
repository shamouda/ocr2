/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_CE_PTHREAD

#include "debug.h"

#include "ocr-policy-domain.h"

#include "ocr-errors.h"
#include "ocr-sysboot.h"
#include "ocr-hal.h"
#include "utils/ocr-utils.h"

#include "ce-pthread-comm-platform.h"
#include "comm-platform/xe-pthread/xe-pthread-comm-platform.h"
#include "policy-domain/ce/ce-policy.h"

#include "xstg-map.h"

#define DEBUG_TYPE COMM_PLATFORM

// Poll for XEs twice as many times as for CEs
#define INQUEUE_POLL_COUNT 2
#define INQUEUECE_POLL_COUNT 1

void cePthreadCommDestruct (ocrCommPlatform_t * base) {
    runtimeChunkFree((u64)base, NULL);
}

u8 cePthreadCommSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
            if(RL_IS_FIRST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // We need at least two phases in NETWORK_OK
                RL_ENSURE_PHASE_UP(PD, RL_NETWORK_OK, RL_PHASE_COMMAPI, 2);
            } else if(RL_IS_LAST_PHASE_UP(PD, RL_CONFIG_PARSE, phase)) {
                // Double check requirements are taken into account
                if(RL_GET_PHASE_COUNT_UP(PD, RL_NETWORK_OK) < 2) {
                    DPRINTF(DEBUG_LVL_WARN, "CE comm-platform needs at least two RL_NETWORK_OK phases\n");
                    ASSERT(0);
                }
                // This assumes that there is only one NODE master because we use
                // the implicit barrier that exists between RL due to only one
                // thread bringing everyone up to PD_OK. We might have to
                // rethink the design for multi-node. This should also
                // ideally be in RL_NETWORK_OK but needs to be across
                // RL to make use of the implicit barrier.
                // We also do this now because we assume that the PD will have
                // discovered neighbors in the first phase
                u32 numSlotsOut, numSlotsXE, numSlotsCE;

                ocrCommPlatformCePthread_t * rself = (ocrCommPlatformCePthread_t*) self;
                // Allow everyone of our neighbor to be able to talk to us at least once
                // Note that the queue always "wastes" one spot so we always use +1
                numSlotsXE = ((ocrPolicyDomainCe_t*)PD)->xeCount;
                numSlotsCE = PD->neighborCount;
                numSlotsOut = numSlotsXE + numSlotsCE;
                numSlotsCE *= 2; // We use two slots per CE so we can allow a back an forth

                // We can only use a maximum of 256 slots due to a restriction on how we
                // encode the slot for the return message
                ASSERT(numSlotsXE+1 < 256);
                ASSERT(numSlotsCE+1 < 256);
                comQueueInit(&(rself->inQueueXE), numSlotsXE+1,
                             (comQueueSlot_t*)runtimeChunkAlloc((numSlotsXE+1)*sizeof(comQueueSlot_t),
                                                                PERSISTENT_CHUNK));
                comQueueInit(&(rself->inQueueCE), numSlotsCE+1,
                             (comQueueSlot_t*)runtimeChunkAlloc((numSlotsCE+1)*sizeof(comQueueSlot_t),
                                                                PERSISTENT_CHUNK));
                DPRINTF(DEBUG_LVL_VERB, "Initialized inQueueXE @ %p of size %"PRIu32" and inQueueCE @ %p of size %"PRIu32"\n",
                        &(rself->inQueueXE), numSlotsXE+1, &(rself->inQueueCE), numSlotsCE+1);
                rself->numOutQueues = numSlotsOut;
                rself->outQueues = (neighborQueue_t*)runtimeChunkAlloc(numSlotsOut*sizeof(neighborQueue_t),
                                                                       PERSISTENT_CHUNK);
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_CONFIG_PARSE, phase)) {
                // Free the structures that were allocated during bring-up
                ocrCommPlatformCePthread_t *rself = (ocrCommPlatformCePthread_t*)self;
                runtimeChunkFree((u64)(rself->inQueueXE.slots), PERSISTENT_CHUNK);
                runtimeChunkFree((u64)(rself->inQueueCE.slots), PERSISTENT_CHUNK);
                runtimeChunkFree((u64)(rself->outQueues), PERSISTENT_CHUNK);
            }
        }
        break;
    case RL_NETWORK_OK:
        if(properties & RL_BRING_UP) {
            if (RL_IS_FIRST_PHASE_UP(PD, RL_NETWORK_OK, phase)) {
                // At this point, given the comment in RL_CONFIG_OK, we can assume that the
                // remote channels are set up and we can use them
                // The PD will also have set up neighborPDs
                u32 i;
                u32 xeCount = ((ocrPolicyDomainCe_t*)PD)->xeCount;;
                ocrCommPlatformCePthread_t * rself = (ocrCommPlatformCePthread_t*) self;
                ASSERT(rself->numOutQueues == PD->neighborCount + xeCount);
                for(i=0; i<xeCount; ++i) {
                    ocrPolicyDomain_t *neighborPD = PD->neighborPDs[i];
                    ocrCommPlatformXePthread_t *neighbor =
                        (ocrCommPlatformXePthread_t*)(neighborPD->commApis[0]->commPlatform);
                    rself->outQueues[i].neighborLocation = neighborPD->myLocation;
                    rself->outQueues[i].outQueue = &(neighbor->inQueue);
                    DPRINTF(DEBUG_LVL_VERB, "Location 0x%"PRIx64": Outqueue for XE 0x%"PRIx64" is @ %p\n",
                            PD->myLocation, neighborPD->myLocation, &(neighbor->inQueue));
                }
                for(i=xeCount; i<PD->neighborCount + xeCount; ++i) {
                    ocrPolicyDomain_t * neighborPD = PD->neighborPDs[i];
                    ocrCommPlatformCePthread_t * neighbor =
                        (ocrCommPlatformCePthread_t *)(neighborPD->commApis[0]->commPlatform);
                    rself->outQueues[i].neighborLocation = neighborPD->myLocation;
                    rself->outQueues[i].outQueue = &(neighbor->inQueueCE);
                    DPRINTF(DEBUG_LVL_VERB, "Location 0x%"PRIx64": Outqueue for CE 0x%"PRIx64" is @ %p\n",
                            PD->myLocation, neighborPD->myLocation, &(neighbor->inQueueCE));
                }
            }
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

    return 0;
}

u8 cePthreadCommSendMessage(ocrCommPlatform_t *self, ocrLocation_t target, ocrPolicyMsg_t *msg,
                            u64 *id, u32 properties, u32 mask) {
    u32 queueIdx, outSlot, inSlot;
    u8 status;
    ocrCommPlatformCePthread_t * rself = (ocrCommPlatformCePthread_t*)self;
    ASSERT(target != self->pd->myLocation);

    // First we find the proper queue to send to
    comQueue_t *queue = NULL, *returnQueue = NULL;
    for(queueIdx=0; queueIdx<rself->numOutQueues; ++queueIdx) {
        if(rself->outQueues[queueIdx].neighborLocation == target) {
            queue = rself->outQueues[queueIdx].outQueue;
            break;
        }
    }
    // We don't know how to send to this neighbor
    if(queue == NULL) {
        DPRINTF(DEBUG_LVL_WARN, "Cannot send to 0x%"PRIx64": no queue to destination\n", target);
        ASSERT(0);
        return OCR_EINTR;
    }
    DPRINTF(DEBUG_LVL_VERB, "Sending msg %p (type: 0x%"PRIx32") to 0x%"PRIx64"; using out queue @ %p\n",
            msg, msg->type, target, queue);

    if((msg->type & (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) == (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) {
        // If this is a request that requires a response, we
        // will pre-reserve a slot on our incoming queue so that
        // the response can come back in. This is to avoid
        // deadlock situations (simple case: both CEs send to one another
        // simultaneously and then can't answer each other so they lock up

        if(AGENT_FROM_ID(target) == ID_AGENT_CE) {
            returnQueue = &(rself->inQueueCE);
        } else {
            returnQueue = &(rself->inQueueXE);
        }

        status = comQueueReserveSlot(returnQueue, &inSlot);
        if(status == 0) {
            // We encode the queue address and slot to be able to make sure
            // things are OK on the way back
            msg->msgId = (((u64)returnQueue) << 8) + inSlot;
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
        // We can't just be sending responses to no questions
        ASSERT(msg->type & (PD_MSG_REQ_RESPONSE | PD_MSG_RESPONSE_OVERRIDE));
        msg->type &= ~PD_MSG_RESPONSE_OVERRIDE;

        // Check if the queue matches (ie: we are sending back where we reserved)
        if((((u64)queue) << 8) != (msg->msgId & 0xFFFFFFFFFFFFFF00ULL)) {
            DPRINTF(DEBUG_LVL_WARN, "Expected to send response to queue 0x%"PRIx64" but found queue @ %p\n",
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
        // If this is a two-way message and a response is therefore required, we are
        // also going to make sure that there is a possibility for the response to come
        // back

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
            *id = (u64)queueIdx << 32 | outSlot;
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


static u8 internalPoll(ocrCommPlatformCePthread_t *rself, ocrPolicyMsg_t **msg, u32 properties,
                u32 *mask, comQueue_t* toPollQueue) {
    u32 slot;
    u8 status;
    status = comQueueReadSlot(toPollQueue, &slot);
    if(status == 0) {
        // We found a message
        u64 baseSize = 0, marshalledSize = 0;
        ocrPolicyMsg_t *curMsg = NULL;
        u8 fixupPtrs = 0;
        if(toPollQueue->slots[slot].msgPtr == NULL) {
            // We have to use the buffer
            curMsg = &(toPollQueue->slots[slot].msgBuffer);
            DPRINTF(DEBUG_LVL_VERB, "Found a message (in buffer) @ %p (type: 0x%"PRIx32")\n",
                    curMsg, curMsg->type);
            fixupPtrs = 1; // We need to unmarshall everytime
        } else {
            curMsg = toPollQueue->slots[slot].msgPtr;
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
                if(fixupPtrs)
                    ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg, MARSHALL_APPEND);
            }
            // In both cases, we can empty the current slot as we have
            // a safe copy of the message
            // Free the message if we have to (persistent and not two ways)
            if(toPollQueue->slots[slot].properties & COMQUEUE_FREE_PTR) {
                rself->base.pd->fcts.pdFree(rself->base.pd, curMsg);
            }
            DPRINTF(DEBUG_LVL_VERB, "Freeing slot %"PRIu32" of queue @ %p (at poll)\n",
                    slot, toPollQueue);
            toPollQueue->slots[slot].properties = 0;
            comQueueEmptySlot(toPollQueue, slot);
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
            toPollQueue->slots[slot].msgPtr = curMsg;
            toPollQueue->slots[slot].properties |= COMQUEUE_EMPTY_PENDING;
        }
        // We do not check if there are more messages (for now)
        DPRINTF(DEBUG_LVL_VERB, "Returning message %p from 0x%"PRIx64"\n", *msg, (*msg)->srcLocation);
        return 0;
    } else if(status == OCR_ENOMEM) {
        return OCR_EINTR<<4; // This is not great because we can't encode full error codes
    } else {
        // This means there is no message yet
        return POLL_NO_MESSAGE;
    }
}

u8 cePthreadCommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                            u32 properties, u32 *mask) {
    ocrCommPlatformCePthread_t *rself = (ocrCommPlatformCePthread_t*)self;

    // We look in our inQueueXE and inQueueCE for a message
    // Pick the first queue to look at
    comQueue_t *toPollQueue = NULL;
    u8 status;
    if(rself->curPollCount & 0x80000000) {
        if((rself->curPollCount & 0xEFFFFFFF) < INQUEUE_POLL_COUNT) {
            ++rself->curPollCount;
            toPollQueue = &(rself->inQueueXE);
        } else {
            rself->curPollCount = 1;
            toPollQueue = &(rself->inQueueCE);
        }
    } else {
        if((rself->curPollCount & 0xEFFFFFFF) < INQUEUECE_POLL_COUNT) {
            ++rself->curPollCount;
            toPollQueue = &(rself->inQueueCE);
        } else {
            rself->curPollCount = 0x80000001;
            toPollQueue = &(rself->inQueueXE);
        }
    }

    // Now do the actual polling
    if(toPollQueue == &(rself->inQueueXE)) {
        // DPRINTF(DEBUG_LVL_VVERB, "Polling XE queue @ 0x%"PRIx64"\n", toPollQueue);
        if((status = internalPoll(rself, msg, properties, mask, toPollQueue)) != 0) {
            rself->curPollCount = 1;
            toPollQueue = &(rself->inQueueCE);
            // DPRINTF(DEBUG_LVL_VVERB, "Switching to poll CE queue @ 0x%"PRIx64"\n", toPollQueue);
            return internalPoll(rself, msg, properties, mask, toPollQueue);
        } else {
            return status;
        }
    } else {
        // DPRINTF(DEBUG_LVL_VVERB, "Polling CE queue @ 0x%"PRIx64"\n", toPollQueue);
        if((status = internalPoll(rself, msg, properties, mask, toPollQueue)) != 0) {
            rself->curPollCount = 0x80000001;
            toPollQueue = &(rself->inQueueXE);
            // DPRINTF(DEBUG_LVL_VVERB, "Switching to poll XE queue @ 0x%"PRIx64"\n", toPollQueue);
            return internalPoll(rself, msg, properties, mask, toPollQueue);
        } else {
            return status;
        }
    }
}

u8 cePthreadCommWaitMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                            u32 properties, u32 *mask) {
    while (cePthreadCommPollMessage(self, msg, properties, mask) != 0)
        ;
    return 0;
}

u64 cePthreadGetSeqIdAtNeighbor(ocrCommPlatform_t *self, ocrLocation_t neighborLoc, u64 neighborId) {
    ASSERT(0); // Not used
    return 0;
}

u8 cePthreadDestructMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t *msg) {
    // Here we need to see if we have a slot to free. We need to look
    // for COMQUEUE_EMPTY_PENDING
    ocrCommPlatformCePthread_t *rself = (ocrCommPlatformCePthread_t*)self;
    u32 i;
    for(i=0; i<rself->inQueueXE.size; ++i) {
        if((rself->inQueueXE.slots[i].properties & COMQUEUE_EMPTY_PENDING) &&
           (rself->inQueueXE.slots[i].msgPtr == msg)) {
            if(rself->inQueueXE.slots[i].properties & COMQUEUE_FREE_PTR) {
                self->pd->fcts.pdFree(self->pd, rself->inQueueXE.slots[i].msgPtr);
            }
            DPRINTF(DEBUG_LVL_VERB, "Freeing slot %"PRIu32" of queue @ %p\n",
                    i, &(rself->inQueueXE));
            rself->inQueueXE.slots[i].properties = 0;
            comQueueEmptySlot(&(rself->inQueueXE), i);
            return 0;
        }
    }
    for(i=0; i<rself->inQueueCE.size; ++i) {
        if((rself->inQueueCE.slots[i].properties & COMQUEUE_EMPTY_PENDING) &&
           (rself->inQueueCE.slots[i].msgPtr == msg)) {
            if(rself->inQueueCE.slots[i].properties & COMQUEUE_FREE_PTR) {
                self->pd->fcts.pdFree(self->pd, rself->inQueueCE.slots[i].msgPtr);
            }
            DPRINTF(DEBUG_LVL_VERB, "Freeing slot %"PRIu32" of queue @ %p\n",
                    i, &(rself->inQueueCE));
            rself->inQueueCE.slots[i].properties = 0;
            comQueueEmptySlot(&(rself->inQueueCE), i);
            return 0;
        }
    }
    DPRINTF(DEBUG_LVL_WARN, "Could not find message %p to destroy\n",
            msg);
    return OCR_EINVAL;
}

ocrCommPlatform_t* newCommPlatformCePthread(ocrCommPlatformFactory_t *factory,
        ocrParamList_t *perInstance) {

    ocrCommPlatformCePthread_t * commPlatformCePthread = (ocrCommPlatformCePthread_t*)
            runtimeChunkAlloc(sizeof(ocrCommPlatformCePthread_t), PERSISTENT_CHUNK);
    ocrCommPlatform_t * base = (ocrCommPlatform_t *) commPlatformCePthread;
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeCommPlatformCePthread(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * base, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, base, perInstance);
    ocrCommPlatformCePthread_t *rself = (ocrCommPlatformCePthread_t*) base;
    rself->numOutQueues = 0;
    rself->outQueues = NULL;
    rself->curPollCount = 0;
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCommPlatformFactoryCePthread(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrCommPlatformFactory_t *newCommPlatformFactoryCePthread(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryCePthread_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newCommPlatformCePthread;
    base->initialize = &initializeCommPlatformCePthread;
    base->destruct = &destructCommPlatformFactoryCePthread;

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), cePthreadCommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                  phase_t, u32, void (*)(ocrPolicyDomain_t*,u64), u64), cePthreadCommSwitchRunlevel);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrLocation_t, ocrPolicyMsg_t *, u64*, u32, u32),
                                     cePthreadCommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t **, u32, u32*),
                                     cePthreadCommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t **, u32, u32*),
                                     cePthreadCommWaitMessage);
    base->platformFcts.destructMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t*),
                                                   cePthreadDestructMessage);
    base->platformFcts.getSeqIdAtNeighbor = FUNC_ADDR(u64 (*)(ocrCommPlatform_t*, ocrLocation_t, u64),
                                                   cePthreadGetSeqIdAtNeighbor);

    return base;
}
#endif /* ENABLE_COMM_PLATFORM_CE_PTHREAD */
