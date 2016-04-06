/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_CE

#include "debug.h"

#include "ocr-comp-platform.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"

#include "policy-domain/ce/ce-policy.h"

#include "utils/ocr-utils.h"

#include "ce-comm-platform.h"

#include "mmio-table.h"
#include "xstg-map.h"

#define DEBUG_TYPE COMM_PLATFORM

u32 XeIrqReq[8];

//
// Hgh-Level Theory of Operation / Design
//pp
// Communication will always involve one local->remote copy of
// information. Whether it is a source initiated bulk DMA or a series
// of remote initiated loads, it *will* happen. What does *not* need
// to happen are any additional local copies between the caller and
// callee on the sending end.
//
// Hence:
//
// (a) Every XE has a local receive stage. Every CE has per-XE receive
//     stage.  All receive stages are at MSG_QUEUE_OFFT in the agent
//     scratchpad and are MSG_QUEUE_SIZE bytes.
//
// (b) Every receive stage starts with an F/E word, followed by
//     content.
//
// (c) ceCommSendMessage():
//
//     CEs do not initiate communication, they only respond. Hence,
//     Send() ops do not expect a reply (they *are* replies
//     themselves) and so they will always be synchronous and once
//     data has been shipped out the buffer passed into the Send() is
//     free to be reused.
//
//        - Atomically test & set remote stage to F. Error if already F.
//        - DMA to remote stage
//        - Send() returns.
//
//     NOTE: XE software needs to consume a response from its stage
//           before injecting another request to CE. Otherwise, there
//           is the possibility of a race in the likely case that the
//           netowrk & CE are much faster than an XE...
//
// (d) ceCommPollMessage() -- non-blocking receive
//
//     Check local stage's F/E word. If E, return empty. If F, return content.
//
// (e) ceCommWaitMessage() -- blocking receive
//
//     While local stage E, keep looping. BUG #618: Should we add a rate limit
//     Once it is F, return content.
//
// (f) ceCommDestructMessage() -- callback to notify received message was consumed
//
//     Atomically test & set local stage to E. Error if already E.
//

// Ugly globals below, but would go away once FSim has QMA support trac #232

// Special values in MSG_CE_ADDR_OFF: EMPTY_SLOT means it can be written to and
// RESERVED_SLOT means someone is holding it for writing later. Both are invalid
// addresses so there should be no conflict
#define EMPTY_SLOT 0x0ULL
#define RESERVED_SLOT 0x1ULL

static void releaseXE(u32 i) {
    DPRINTF(DEBUG_LVL_VERB, "Ungating XE %"PRIu32"\n", i);
    // Bug #820: This was a MMIO LD call and should be replaced by one when they become available
    // The XE should be clock-gated already because we don't process its message before it is
    ASSERT(*((volatile u64*)(BR_MSR_BASE((i+ID_AGENT_XE0)) + (POWER_GATE_RESET * sizeof(u64)))) & 0x1ULL);

    // Bug #820: Further, this was a MMIO operation
    *((u64*)(BR_MSR_BASE((i+ID_AGENT_XE0)) + (POWER_GATE_RESET * sizeof(u64)))) &= ~(0x1ULL);
    DPRINTF(DEBUG_LVL_VERB, "XE %"PRIu32" ungated\n", i);
}

static u8 ceCommSendMessageToCE(ocrCommPlatform_t *self, ocrLocation_t target,
                                ocrPolicyMsg_t *message, u64 *id,
                                u32 properties, u32 mask) {

    u32 i;
    u64 retval, msgAbsAddr;
    u64* rmbox;
    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsg_t *sendBuf = NULL;

    ASSERT(self->location != target);

    // We look for an empty buffer to use
    ocrPolicyMsg_t *buffers = (ocrPolicyMsg_t*)(AR_L1_BASE + MSG_CE_SEND_BUF_OFFT);
    for(i=0; i<OUTSTANDING_CE_SEND; ++i, ++buffers) {
        if(buffers->type == 0) {
            // Found one
            sendBuf = buffers;
            DPRINTF(DEBUG_LVL_VERB, "Using local buffer %"PRIu32" @ %p\n", i, sendBuf);
            break;
        }
    }
    if(sendBuf == NULL) {
        // This means that the buffer for a previous send is
        // still in use. We need to wait until we can send a
        // message to another CE for now
        DPRINTF(DEBUG_LVL_VERB, "Local buffers all busy for CE->CE message %p ID 0x%"PRIx64" (type 0x%"PRIx32") from 0x%"PRIx64" to 0x%"PRIx64"\n",
                message, message->msgId, message->type, self->location, target);
        return OCR_EBUSY;
    }

    // At this point, sendBuf is available to use
    // Check size of message

    ocrPolicyMsgGetMsgSize(message, &baseSize, &marshalledSize, 0);
    if(baseSize + marshalledSize > sendBuf->bufferSize) {

        DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %"PRIu64" (%p)\n",
                sendBuf->bufferSize, sendBuf);
        ASSERT(0);
    }

    // Figure out where our remote boxes our (where we are sending to)
    rmbox = (u64 *) (SR_L1_BASE(CLUSTER_FROM_ID(target), BLOCK_FROM_ID(target), AGENT_FROM_ID(target))
                     + MSG_CE_ADDR_OFFT);

    // Calculate our absolute sendBuf address
    msgAbsAddr = SR_L1_BASE(CLUSTER_FROM_ID(self->location), BLOCK_FROM_ID(self->location),
                            AGENT_FROM_ID(self->location))
        + ((u64)(sendBuf) - AR_L1_BASE);

    // We now check to see if the message requires a response, if so, we will reserve
    // the response slot
    bool reservedSlot = false;
    if((message->type & (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) == (PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE)) {
        reservedSlot = true;
        u64 *lmbox = (u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT);
        retval = RESERVED_SLOT;
        for(i=0; i<OUTSTANDING_CE_MSGS; ++i) {
            retval = hal_cmpswap64(&(lmbox[i]), EMPTY_SLOT, RESERVED_SLOT);
            if(retval == EMPTY_SLOT)
                break;
        }
        if(retval == EMPTY_SLOT) {
            DPRINTF(DEBUG_LVL_VERB, "Message requires a response, reserved slot %"PRIu32" on local queue for 0x%"PRIx64"\n", i, self->location);
            message->msgId = (self->location << 8) + i;
        } else {
            DPRINTF(DEBUG_LVL_VERB, "Message requires a response but local return queue is busy\n");
            return OCR_EBUSY;
        }
    }

    // If this is a response, we should already have a slot reserved for us
    if(message->type & (PD_MSG_RESPONSE | PD_MSG_RESPONSE_OVERRIDE)) {
        // We can't just be sending responses to no questions
        ASSERT(message->type & (PD_MSG_REQ_RESPONSE | PD_MSG_RESPONSE_OVERRIDE));
        message->type &= ~PD_MSG_RESPONSE_OVERRIDE;

        // Make sure we are sending back to the right CE
        if((message->msgId &= ~0xFFULL) != (target << 8)) {
            DPRINTF(DEBUG_LVL_WARN, "Expected to send response to 0x%"PRIx64" but read msgId 0x%"PRIx64" (location: 0x%"PRIx64")\n",
                    target, message->msgId, message->msgId >> 8);
            ASSERT(0);
        }

        DPRINTF(DEBUG_LVL_VERB, "Using pre-reserved slot %"PRIu64" to send to 0x%"PRIx64"\n", message->msgId & 0xFF,
                target);

        // Actually marshall the message
        ocrPolicyMsgMarshallMsg(message, baseSize, (u8 *)sendBuf, MARSHALL_FULL_COPY);
        // And now set the slot propertly (from reserved to the address)
        RESULT_ASSERT(hal_cmpswap64(&(rmbox[message->msgId & 0xFF]), RESERVED_SLOT, msgAbsAddr), ==, RESERVED_SLOT);
    } else {
        // We need to find a slot to send to. We first reserve to save on marshalling if unsuccessful
        for(i=0; i<OUTSTANDING_CE_MSGS; ++i) {
            retval = hal_cmpswap64(&rmbox[i], EMPTY_SLOT, RESERVED_SLOT);
            if(retval == EMPTY_SLOT)
                break; // Send successful
        }
        if(retval != EMPTY_SLOT) {
            DPRINTF(DEBUG_LVL_VERB, "Target CE busy for CE->CE message %p (type 0x%"PRIx32") from 0x%"PRIx64" to 0x%"PRIx64"\n",
                    message, message->type, self->location, target);
            if(reservedSlot) {
                // We free the slot we had reserved
                RESULT_ASSERT(hal_cmpswap64(&(rmbox[message->msgId & 0xFF]), RESERVED_SLOT, EMPTY_SLOT), ==, RESERVED_SLOT);
            }
            return OCR_EBUSY;
        } else {
            // We can now marshall the message
            ocrPolicyMsgMarshallMsg(message, baseSize, (u8 *)sendBuf, MARSHALL_FULL_COPY);
            // And now set the slot propertly (from reserved to the address)
            RESULT_ASSERT(hal_cmpswap64(&(rmbox[i]), RESERVED_SLOT, msgAbsAddr), ==, RESERVED_SLOT);
        }
    }
    DPRINTF(DEBUG_LVL_VERB, "Sent CE->CE message %p ID 0x%"PRIx64" (type 0x%"PRIx32") from 0x%"PRIx64" to 0x%"PRIx64" slot %"PRIu32" using buffer %p\n",
            message, message->msgId, message->type, self->location, target, i, sendBuf);
    return 0;
}

static u8 ceCommDestructCEMessage(ocrCommPlatform_t *self, u32 idx) {

    ASSERT(idx < OUTSTANDING_CE_MSGS);
    DPRINTF(DEBUG_LVL_VERB, "Destructing message index %"PRIu32" (0x%"PRIx64")\n", idx, *(u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT + sizeof(u64)*idx));

    ocrPolicyMsg_t *msg = (ocrPolicyMsg_t*)(*(u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT + sizeof(u64)*idx));
    *(u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT + sizeof(u64)*idx) = EMPTY_SLOT;
    msg->type = 0;
    msg->msgId = (u64)-1;

    return 0;
}

static u8 ceCommCheckCEMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg) {
    u32 j;

    ocrPolicyMsg_t *recvBuf = (ocrPolicyMsg_t*)(AR_L1_BASE + MSG_CE_RECV_BUF_OFFT);
    // Go through our mbox to check for valid messages
    if(recvBuf->type) {
        // Receive buffer is busy
        DPRINTF(DEBUG_LVL_VERB, "Receive buffer @ %p is busy, cannot receive CE messages\n", recvBuf);
        return POLL_NO_MESSAGE;
    }
    for(j=0; j<OUTSTANDING_CE_MSGS; ++j) {
        u64 addr = *(u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT + sizeof(u64)*j);
        if((addr != EMPTY_SLOT) &&
           (addr != RESERVED_SLOT)) {
            DPRINTF(DEBUG_LVL_VERB, "Found an incoming CE message (0x%"PRIx64") @ idx %"PRIu32"\n", addr, j);
            // We fixup pointers
            u64 baseSize = 0, marshalledSize = 0;
            ocrPolicyMsgGetMsgSize((ocrPolicyMsg_t*)addr, &baseSize, &marshalledSize, 0);
            if(baseSize + marshalledSize > recvBuf->bufferSize) {
                DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %"PRId64"\n",
                                         recvBuf->bufferSize);
                ASSERT(0);
            }
            ocrPolicyMsgUnMarshallMsg((u8*)addr, NULL, recvBuf, MARSHALL_FULL_COPY);
            DPRINTF(DEBUG_LVL_VERB, "Copied message from 0x%"PRIx64" of type 0x%"PRIx32" to receive buffer @ %p\n",
                    recvBuf->srcLocation, recvBuf->type, recvBuf);
            ceCommDestructCEMessage(self, j);
            *msg = recvBuf;
            return 0;
        }
    }
    *msg = NULL;
    return POLL_NO_MESSAGE;
}

void ceCommDestruct (ocrCommPlatform_t * base) {

    runtimeChunkFree((u64)base, NULL);
}

u8 ceCommSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                      phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

#ifndef ENABLE_BUILDER_ONLY

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
    {
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_NETWORK_OK, phase)) {
            u32 i;
            ocrCommPlatformCe_t *cp = (ocrCommPlatformCe_t*)self;

            // Figure out our location
            self->location = PD->myLocation;

            // Initialize the bufferSize properly for recvBuf sendBuf
            initializePolicyMessage((ocrPolicyMsg_t*)(AR_L1_BASE + MSG_CE_RECV_BUF_OFFT), sizeof(ocrPolicyMsg_t));
            ocrPolicyMsg_t *buffers = (ocrPolicyMsg_t*)(AR_L1_BASE + MSG_CE_SEND_BUF_OFFT);
            for(i=0; i<OUTSTANDING_CE_SEND; ++i, ++buffers) {
                initializePolicyMessage(buffers, sizeof(ocrPolicyMsg_t));
            }

            // Pre-compute pointer to our block's XEs' remote stages (where we send to)
            // Pre-compute pointer to our block's XEs' local stages (where they send to us)
            COMPILE_TIME_ASSERT(ID_AGENT_XE0 == 1); // This loop assumes this. Fix if this changes
            for(i=0; i< ((ocrPolicyDomainCe_t*)PD)->xeCount; ++i) {
                cp->rq[i] = (u64 *)(BR_L1_BASE(i+ID_AGENT_XE0) + MSG_QUEUE_OFFT);
                cp->lq[i] = (u64 *)(AR_L1_BASE + (u64)MSG_QUEUE_OFFT + i * MSG_QUEUE_SIZE);
                *(cp->rq[i]) = 0; // Only initialize the remote one; XEs initialize local ones
            }

            // Arbitrary first choice for the queue
            cp->pollq = 0;

            // Initialize things
            for(i = 0; i < OUTSTANDING_CE_MSGS; ++i) {
                *(u64*)(AR_L1_BASE + MSG_CE_ADDR_OFFT + i*sizeof(u64)) = EMPTY_SLOT;
            }

            // Statically check stage area is big enough for 1 policy message + F/E word
            COMPILE_TIME_ASSERT(MSG_QUEUE_SIZE >= (sizeof(u64) + sizeof(ocrPolicyMsg_t)));
        }
        break;
    }
    case RL_PD_OK:
        if(properties & RL_BRING_UP) {
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
#endif

    return toReturn;
}

u8 ceCommSendMessage(ocrCommPlatform_t *self, ocrLocation_t target,
                     ocrPolicyMsg_t *message, u64 *id,
                     u32 properties, u32 mask) {
    ASSERT(self != NULL);
    ASSERT(message != NULL);
    ASSERT(target != self->location);

    ocrCommPlatformCe_t * cp = (ocrCommPlatformCe_t *)self;

    // If target is not in the same block, use a different function
    // BUG #618: do the same for socket/cluster/board as well, or better yet, a new macro
    if(BLOCK_FROM_ID(self->location) != BLOCK_FROM_ID(target)) {
        return ceCommSendMessageToCE(self, target, message, id, properties, mask);
    } else {
        // BUG #618: compute all-but-agent & compare between us & target
        // Target XE's stage (note this is in remote XE memory!)
        volatile u64 * rq = cp->rq[(AGENT_FROM_ID((u64)target)) - ID_AGENT_XE0];

        // - Check remote stage Empty/Busy/Full is Empty.
        {
            // Bug #820: This was an MMIO gate
            volatile u64 tmp = *((u64*)rq);
            if(tmp) {
                DPRINTF(DEBUG_LVL_VERB, "Sending to 0x%"PRIx64" failed (busy) because %p reads %"PRId64"\n",
                        target, rq, tmp);
                return OCR_EBUSY;
            }
            ASSERT(tmp == 0);
        }

#ifndef ENABLE_BUILDER_ONLY
        // - DMA to remote stage
        // We marshall things properly
        u64 baseSize = 0, marshalledSize = 0;
        ocrPolicyMsgGetMsgSize(message, &baseSize, &marshalledSize, 0);
        // We can only deal with the case where everything fits in the message
        if(baseSize + marshalledSize > message->bufferSize) {
            DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %"PRId64"\n",
                    message->bufferSize);
            ASSERT(0);
        }
        ocrPolicyMsgMarshallMsg(message, baseSize, (u8*)message, MARSHALL_APPEND);
        if(message->usefulSize > MSG_QUEUE_SIZE - sizeof(u64))
            DPRINTF(DEBUG_LVL_WARN, "Message of type %"PRIx32" is too large (%"PRIx64") for buffer (%"PRIx64")\n",
                                    message->type, message->usefulSize, MSG_QUEUE_SIZE-sizeof(u64));
        ASSERT(message->usefulSize <= MSG_QUEUE_SIZE - sizeof(u64));
        // - DMA to remote stage, with fence
        DPRINTF(DEBUG_LVL_VVERB, "DMA-ing out message to 0x%"PRIx64" of size %"PRId64"\n",
                (u64)&rq[1], message->usefulSize);
        hal_memCopy(&(rq[1]), message, message->usefulSize, true);
        DPRINTF(DEBUG_LVL_VVERB, "DMA done (async)\n");
        // - Fence DMA
        hal_fence();
        DPRINTF(DEBUG_LVL_VVERB, "Past fence\n");
        // - Atomically test & set remote stage to Full. Error if already non-Empty.
        {
            RESULT_ASSERT(hal_swap64(rq, 2), ==, 0);
        }
        DPRINTF(DEBUG_LVL_VVERB, "DMA done and set %p to 2 (full)\n", &(rq[0]));

        if(message->type & (PD_MSG_RESPONSE | PD_MSG_RESPONSE_OVERRIDE)) {
            // Release the XE now that it has a response to see
            // WARNING: we only release if this is a response. If this is an initial message
            // (happens to release from barriers), we do not release (the XE is already released)
            DPRINTF(DEBUG_LVL_VERB, "Releasing XE 0x%"PRIx64" after sending it a response of type 0x%"PRIx32"\n",
                    (AGENT_FROM_ID((u64)target)) - ID_AGENT_XE0, message->type);
            releaseXE((AGENT_FROM_ID((u64)target)) - ID_AGENT_XE0);
        }
#endif
    }
    return 0;
}

static u8 extractXEMessage(ocrCommPlatformCe_t *cp, ocrPolicyMsg_t **msg, u32 queueIdx) {
    // We have a message
    *msg = (ocrPolicyMsg_t *)&((cp->lq[queueIdx])[1]);
    DPRINTF(DEBUG_LVL_VERB, "Found message from XE 0x%"PRIx32" @ %p of type 0x%"PRIx32"\n",
            queueIdx, *msg, (*msg)->type);
    // We fixup pointers
    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsgGetMsgSize(*msg, &baseSize, &marshalledSize, 0);
    if(baseSize + marshalledSize > (*msg)->bufferSize) {
        DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %"PRId64"\n",
                (*msg)->bufferSize);
        ASSERT(0);
    }
    ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg, MARSHALL_APPEND);
    cp->lq[queueIdx][0] = 3; // Signify we are reading it so we don't read it twice if we
                             // go back in to poll before we destroyed this message (if, for
                             // example, we are stuck sending a needed message to a CE)
    // We also un-clockgate the XE if no response is expected
    hal_fence();
    if(!((*msg)->type & PD_MSG_REQ_RESPONSE)) {
        DPRINTF(DEBUG_LVL_VERB, "Message type 0x%"PRIx32" does not require a response -- un-clockgating XE 0x%"PRIx32"\n",
                (*msg)->type, queueIdx);
        releaseXE(queueIdx);
    }
    return 0;
}

u8 ceCommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                     u32 properties, u32 *mask) {

    ASSERT(self != NULL);
    ASSERT(msg != NULL);

    ocrCommPlatformCe_t * cp = (ocrCommPlatformCe_t *)self;

    // First check for CE messages
    if(ceCommCheckCEMessage(self, msg) == 0) {
        return 0;
    }

    // Here, we do not have a CE message so we look for XE messages
    // Local stage is at well-known 0x0
    u32 i, j;

    // Loop through the stages till we receive something
    for(i = cp->pollq, j=(cp->pollq - 1 + ((ocrPolicyDomainCe_t*)self->pd)->xeCount) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount;
        XeIrqReq[i] == 0; i = (i+1) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount) {

        // Halt the CPU, instead of burning rubber
        // An alarm would wake us, so no delay will result
        // Note that a timer alarm wakes us up periodically
        if(i==j) {
            // Try to be fair to all XEs (somewhat anyways)
            cp->pollq = (i + 1) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount;
            return POLL_NO_MESSAGE;
        }
    }
    // If we found a message it means that cp->lq[i][0] should be 2
    // We now rely on the XeIrqReq vector to tell us if we have a message
    // but we should still look to make sure we actually have a message
    ASSERT(cp->lq[i][0] == 2);
    // We also reset the IRQ vector here (just to say that we saw the alarm)
    ASSERT(XeIrqReq[i] == 1);
    XeIrqReq[i] = 0;
    // Try to be fair to all XEs (somewhat anyways)
    cp->pollq = (i + 1) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount;
    // One message being returned
    return extractXEMessage(cp, msg, i);
}

u8 ceCommWaitMessage(ocrCommPlatform_t *self,  ocrPolicyMsg_t **msg,
                     u32 properties, u32 *mask) {

    ASSERT(self != NULL);
    ASSERT(msg != NULL);

    ocrCommPlatformCe_t * cp = (ocrCommPlatformCe_t *)self;

    // First check for CE messages
    if(ceCommCheckCEMessage(self, msg) == 0) {
        return 0;
    }

    // Here, we do not have a CE message so we look for XE messages
    // Local stage is at well-known 0x0
    u32 i, j;

    DPRINTF(DEBUG_LVL_VVERB, "Going to wait for message (starting at %"PRId64")\n",
            cp->pollq);
    // Loop through the stages till we receive something
    for(i = cp->pollq, j=(cp->pollq - 1 + ((ocrPolicyDomainCe_t*)self->pd)->xeCount) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount;
           XeIrqReq[i] == 0; i = (i+1) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount) {
        // Halt the CPU, instead of burning rubber
        // An alarm would wake us, so no delay will result
        // Note that a timer alarm wakes us up periodically
        if(i==j) {
            // Check again for a CE message just in case
            if(!ceCommCheckCEMessage(self, msg))
                return 0;
            __asm__ __volatile__("hlt\n\t");
        }
    }

    // If we found a message it means that cp->lq[i][0] should be 2
    // We now rely on the XeIrqReq vector to tell us if we have a message
    // but we should still look to make sure we actually have a message
    ASSERT(cp->lq[i][0] == 2);
    // We also reset the IRQ vector here (just to say that we saw the alarm)
    ASSERT(XeIrqReq[i] == 1);
    XeIrqReq[i] = 0;
    // Try to be fair to all XEs (somewhat anyways)
    cp->pollq = (i + 1) % ((ocrPolicyDomainCe_t*)self->pd)->xeCount;
    // One message being returned
    return extractXEMessage(cp, msg, i);
}

u8 ceCommDestructMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t *msg) {
    ocrCommPlatformCe_t * cp = (ocrCommPlatformCe_t *)self;
    u32 i;

    ocrPolicyMsg_t *recvBuf = (ocrPolicyMsg_t*)(AR_L1_BASE + MSG_CE_RECV_BUF_OFFT);
    if(msg == recvBuf) {
        // This is an incomming message from a CE, we say that we
        // are done with it so we can receive the next message
        DPRINTF(DEBUG_LVL_VVERB, "Destroying recvBuf @ %p\n", recvBuf);
        recvBuf->type = 0;
        return 0;
    } else {
        // We look for a message from the XE and un-clockgate it
        for(i=0; i < ((ocrPolicyDomainCe_t*)self->pd)->xeCount; ++i) {
            if(msg == (ocrPolicyMsg_t*)&((cp->lq[i])[1])) {
                // We should have been reading it
                ASSERT(cp->lq[i][0] == 3);
                cp->lq[i][0] = 0;
                return 0;
            }
        }
        // If we get here, this means that we have no idea what this message is
        DPRINTF(DEBUG_LVL_WARN, "Unknown message to destroy: %p\n", msg);
        ASSERT(0);
        return OCR_EINVAL;
    }
}

u64 ceGetSeqIdAtNeighbor(ocrCommPlatform_t *self, ocrLocation_t neighborLoc, u64 neighborId) {
    return UNINITIALIZED_NEIGHBOR_INDEX;
}

ocrCommPlatform_t* newCommPlatformCe(ocrCommPlatformFactory_t *factory,
                                     ocrParamList_t *perInstance) {

    ocrCommPlatformCe_t * commPlatformCe = (ocrCommPlatformCe_t*)
                                           runtimeChunkAlloc(sizeof(ocrCommPlatformCe_t), PERSISTENT_CHUNK);
    ocrCommPlatform_t * derived = (ocrCommPlatform_t *) commPlatformCe;
    factory->initialize(factory, derived, perInstance);
    return derived;
}

u8 ceCommSetMaxExpectedMessageSize(ocrCommPlatform_t *self, u64 size, u32 mask) {
    ASSERT(0);
    return 0;
}

void initializeCommPlatformCe(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * derived, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, derived, perInstance);
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCommPlatformFactoryCe(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrCommPlatformFactory_t *newCommPlatformFactoryCe(ocrParamList_t *perType) {

    // Check to make sure we are not going over the start of the heap
    COMPILE_TIME_ASSERT(((MSG_CE_SEND_BUF_OFFT + OUTSTANDING_CE_SEND*sizeof(ocrPolicyMsg_t) + 7) & ~0x7ULL) < 0x1000);

    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryCe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newCommPlatformCe;
    base->initialize = &initializeCommPlatformCe;
    base->destruct = &destructCommPlatformFactoryCe;

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), ceCommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64),
                                                  ceCommSwitchRunlevel);
    base->platformFcts.setMaxExpectedMessageSize = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, u64, u32),
                                                             ceCommSetMaxExpectedMessageSize);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrLocation_t,
                                                      ocrPolicyMsg_t *, u64*, u32, u32), ceCommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               ceCommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               ceCommWaitMessage);
    base->platformFcts.destructMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t*),
                                                   ceCommDestructMessage);
    base->platformFcts.getSeqIdAtNeighbor = FUNC_ADDR(u64 (*)(ocrCommPlatform_t*, ocrLocation_t, u64),
                                                      ceGetSeqIdAtNeighbor);

    return base;
}
#endif /* ENABLE_COMM_PLATFORM_CE */
