/*
 * This file is subject to the lixense agreement located in the file LICENSE
 * and cannot be distributed without it. This notixe cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_XE

#include "debug.h"

#include "ocr-comm-api.h"
#include "ocr-policy-domain.h"

#include "ocr-sysboot.h"
#include "utils/ocr-utils.h"

#include "xe-comm-platform.h"

#include "xe-abi.h"
#include "mmio-table.h"
#include "rmd-arch.h"
#include "rmd-map.h"
#include "rmd-msg-queue.h"

#define DEBUG_TYPE COMM_PLATFORM


//
// Hgh-Level Theory of Operation / Design
//
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
// (c) xeCommSendMessage() has 2 cases:
//
//    (1) Send() of a persistent buffer -- the caller guarantees that
//        the buffer will hang around at least until a response for it
//        has been received.
//
//    (2) Send() of an ephemeral buffer -- the caller can reclaim the
//        buffer as soon as Send() returns to it.
//
//    In either case we:
//
//        - Atomically test & set remote stage to F. Error if already F.
//        - DMA to remote stage
//        - Fence DMA
//        - Alarm remote, freezing
//        - CE IRQ restarts XE clock immediately; Send() returns.
//
// (d) xeCommPollMessage() -- non-blocking receive
//
//     Check local stage's F/E word. If E, return empty. If F, return content.
//
// (e) xeCommWaitMessage() -- blocking receive
//
//     While local stage E, keep looping. (BUG #515: allow XEs to sleep)
//     Once it is F, return content.
//
// (f) xeCommDestructMessage() -- callback to notify received message was consumed
//
//     Atomically test & set local stage to E. Error if already E.
//

void xeCommDestruct (ocrCommPlatform_t * base) {
    runtimeChunkFree((u64)base, NULL);
}

void xeCommBegin(ocrCommPlatform_t * commPlatform, ocrPolicyDomain_t * PD) {

    ASSERT(commPlatform != NULL && PD != NULL);

#ifndef ENABLE_BUILDER_ONLY
    u64 i;
    ocrCommPlatformXe_t * cp = (ocrCommPlatformXe_t *)commPlatform;

    u64 myid = *(u64 *)(XE_MSR_OFFT + CORE_LOCATION * sizeof(u64));

    // Zero-out our stage for receiving messages
    for(i=MSG_QUEUE_OFFT; i<MSG_QUEUE_SIZE; i += sizeof(u64))
        *(volatile u64 *)i = 0;

    // Fill-in location tuples: ours and our parent's (the CE in FSIM)
    PD->myLocation = (ocrLocation_t)myid;
    hal_fence();
    PD->parentLocation = (PD->myLocation & ~ID_AGENT_MASK) | ID_AGENT_CE;

    // Remember our PD in case we need to call through it later
    cp->pdPtr = PD;

    // Remember which XE number we are
    cp->N = (PD->myLocation & ID_AGENT_MASK);

    // Pre-compute pointer to our stage at the CE
    cp->rq = (u64 *)(BR_CE_BASE + MSG_QUEUE_OFFT + cp->N * MSG_QUEUE_SIZE);
#endif
}

void xeInitBarrier(void)
{
    volatile u64 * lq = 0x0;

    lq[0] = 0xfeedf00d;
    // While local stage non-Full, keep looping.
    // BUG #515: Allow XEs to sleep
    while(lq[0] != 2)
        hal_fence();
    lq[0] = 0;
}

u8 xeCommSwitchRunlevel(ocrCommPlatform_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        xeCommBegin(self, PD);
        // Barrier - wait for CE's go signal
        xeInitBarrier();
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

u8 xeCommSendMessage(ocrCommPlatform_t *self, ocrLocation_t target,
                     ocrPolicyMsg_t *message, u64 *id,
                     u32 properties, u32 mask) {

#ifndef ENABLE_BUILDER_ONLY
    // Statically check stage area is big enough for 1 policy message
    COMPILE_TIME_ASSERT(sizeof(ocrPolicyMsg_t) < (MSG_QUEUE_SIZE + sizeof(u64)));

    ASSERT(self != NULL);
    ASSERT(message != NULL && message->bufferSize != 0);

    ocrCommPlatformXe_t * cp = (ocrCommPlatformXe_t *)self;

    // For now, XEs only sent to their CE; make sure!
    if(target != cp->pdPtr->parentLocation)
        DPRINTF(DEBUG_LVL_WARN, "XE trying to send to %lx not parent %lx\n", target, cp->pdPtr->parentLocation);
    ASSERT(target == cp->pdPtr->parentLocation);

    // - Atomically test & set remote stage to Busy. Error if already non-Empty.
    {
        u64 tmp = hal_swap64(cp->rq, (u64)1);
        ASSERT(tmp == 0);
    }

    // We marshall things properly
    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsgGetMsgSize(message, &baseSize, &marshalledSize, 0);
    // We can only deal with the case where everything fits in the message
    if(baseSize + marshalledSize > message->bufferSize) {
        DPRINTF(DEBUG_LVL_WARN, "Message can only be of size %ld got %ld\n",
                message->bufferSize, baseSize + marshalledSize);
        ASSERT(0);
    }
    ocrPolicyMsgMarshallMsg(message, baseSize, (u8*)message, MARSHALL_APPEND);
    if(message->usefulSize > MSG_QUEUE_SIZE - sizeof(u64))
        DPRINTF(DEBUG_LVL_WARN, "Message of type %x has size (%lx) too large (limit %lx)\n",
                                message->type, message->usefulSize, MSG_QUEUE_SIZE-sizeof(u64));
    ASSERT(message->usefulSize <= MSG_QUEUE_SIZE - sizeof(u64))
    // - DMA to remote stage, with fence
    DPRINTF(DEBUG_LVL_VVERB, "DMA-ing out message to 0x%lx of size %d\n",
            &(cp->rq)[1], message->usefulSize);
    hal_memCopy(&(cp->rq)[1], message, message->usefulSize, 0);

    // - Atomically test & set remote stage to Full. Error otherwise (Empty/Busy.)
    {
        u64 tmp = hal_swap64(cp->rq, (u64)2);
        ASSERT(tmp == 1);
    }

    // - Alarm remote to tell the CE it has something (in case it is halted)
    __asm__ __volatile__("alarm %0\n\t" : : "L" (XE_MSG_QUEUE));

#endif

    return 0;
}

u8 xeCommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                     u32 properties, u32 *mask) {

    ASSERT(self != NULL);
    ASSERT(msg != NULL);

    // Local stage is at well-known 0x0
    u64 * lq = 0x0;

    // Check local stage's Empty/Busy/Full word. If non-Full, return; else, return content.
    if(lq[0] != 2) return 0;

#if 1
    // Provide a ptr to the local stage's contents
    *msg = (ocrPolicyMsg_t *)&lq[1];
    if((*msg)->bufferSize > MSG_QUEUE_SIZE - sizeof(u64))
        DPRINTF(DEBUG_LVL_WARN, "Message of type %x has buffer size (%lx) too large (limit %lx)\n",
                                (*msg)->type, (*msg)->bufferSize, MSG_QUEUE_SIZE-sizeof(u64));
    ASSERT((*msg)->bufferSize <= MSG_QUEUE_SIZE - sizeof(u64));
    // We fixup pointers
    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsgGetMsgSize(*msg, &baseSize, &marshalledSize, 0);
    if(baseSize + marshalledSize > (*msg)->bufferSize) {
        DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %ld\n",
                (*msg)->bufferSize);
        ASSERT(0);
    }
    ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg, MARSHALL_APPEND);

#else
    // NOTE: For now we copy it into the buffer provided by the caller
    //       eventually when QMA arrives we'll move to a posted-buffer
    //       scheme and eliminate the copies.
    hal_memCopy(*msg, &lq[1], sizeof(ocrPolicyMsg_t), 0);
    hal_fence();
#endif

    return 0;
}

u8 xeCommWaitMessage(ocrCommPlatform_t *self,  ocrPolicyMsg_t **msg,
                     u32 properties, u32 *mask) {

    ASSERT(self != NULL);
    ASSERT(msg != NULL);

    // Local stage is at well-known 0x0
    volatile u64 * lq = 0x0;

    // While local stage non-Full, keep looping.
    // BUG #515: Sleep
    while(lq[0] != 2);

    // Once it is F, return content.

#if 1
    // Provide a ptr to the local stage's contents
    *msg = (ocrPolicyMsg_t *)&lq[1];
    // We fixup pointers
    u64 baseSize = 0, marshalledSize = 0;
    ocrPolicyMsgGetMsgSize(*msg, &baseSize, &marshalledSize, 0);
    if((*msg)->bufferSize > MSG_QUEUE_SIZE - sizeof(u64))
        DPRINTF(DEBUG_LVL_WARN, "Message of type %x has buffer size (%lx) too large (limit %lx)\n",
                                (*msg)->type, (*msg)->bufferSize, MSG_QUEUE_SIZE-sizeof(u64));
    ASSERT((*msg)->bufferSize <= MSG_QUEUE_SIZE - sizeof(u64));
    if(baseSize + marshalledSize > (*msg)->bufferSize) {
        DPRINTF(DEBUG_LVL_WARN, "Comm platform only handles messages up to size %ld\n",
                (*msg)->bufferSize);
        ASSERT(0);
    }
    ocrPolicyMsgUnMarshallMsg((u8*)*msg, NULL, *msg, MARSHALL_APPEND);
#else
    // NOTE: For now we copy it into the buffer provided by the caller
    //       eventually when QMA arrives we'll move to a posted-buffer
    //       scheme and eliminate the copies.
    hal_memCopy(*msg, &lq[1], sizeof(ocrPolicyMsg_t), 0);
    hal_fence();
#endif

    return 0;
}

u8 xeCommSetMaxExpectedMessageSize(ocrCommPlatform_t *self, u64 size, u32 mask) {
    ASSERT(0);
    return 0;
}

u8 xeCommDestructMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t *msg) {

    ASSERT(self != NULL);
    ASSERT(msg != NULL);
    DPRINTF(DEBUG_LVL_VERB, "Resetting incomming message buffer\n");
#ifndef ENABLE_BUILDER_ONLY
    // Local stage is at well-known 0x0
    u64 * lq = 0x0;
    // - Atomically test & set local stage to Empty. Error if prev not Full.
    {
        u64 old = hal_swap64(lq, 0);
        ASSERT(old == 2);
    }
#endif

    return 0;
}

ocrCommPlatform_t* newCommPlatformXe(ocrCommPlatformFactory_t *factory,
                                     ocrParamList_t *perInstance) {

    ocrCommPlatformXe_t * commPlatformXe = (ocrCommPlatformXe_t*)
                                           runtimeChunkAlloc(sizeof(ocrCommPlatformXe_t), PERSISTENT_CHUNK);
    ocrCommPlatform_t * base = (ocrCommPlatform_t *) commPlatformXe;
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeCommPlatformXe(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * base, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, base, perInstance);
}

/******************************************************/
/* OCR COMP PLATFORM PTHREAD FACTORY                  */
/******************************************************/

void destructCommPlatformFactoryXe(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrCommPlatformFactory_t *newCommPlatformFactoryXe(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
                                     runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryXe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newCommPlatformXe;
    base->initialize = &initializeCommPlatformXe;
    base->destruct = &destructCommPlatformFactoryXe;

    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), xeCommDestruct);
    base->platformFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                         phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), xeCommSwitchRunlevel);
    base->platformFcts.setMaxExpectedMessageSize = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, u64, u32),
                                                             xeCommSetMaxExpectedMessageSize);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrLocation_t,
                                                      ocrPolicyMsg_t *, u64*, u32, u32), xeCommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               xeCommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t**, u32, u32*),
                                               xeCommWaitMessage);
    base->platformFcts.destructMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*, ocrPolicyMsg_t*),
                                                   xeCommDestructMessage);

    return base;
}
#endif /* ENABLE_COMM_PLATFORM_XE */
