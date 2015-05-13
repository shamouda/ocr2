
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COMM_PLATFORM_GASNET
#ifdef COMM_PLATFORM_GASNET_AMLONG


#include <assert.h>
#include <gasnet.h>

#include "ocr-policy-domain.h"

#include "debug.h"

#include "gasnet-comm-platform.h"
#include "gasnet-policy-domain.h"
#include "gasnet-message.h"

#include "gasnet-am.h"
#include "amhandler.h"

#include "gasnet-message-split.h"

// --------------------------------------------------------------------------------------
// variables
// --------------------------------------------------------------------------------------
static FctMessageIncoming fctIncomingMessage;


// --------------------------------------------------------------------------------------
// prototypes declaration
// --------------------------------------------------------------------------------------

void gasnetAMMessageLong(gasnet_token_t token, void *buf, size_t nbytes,
                         gasnet_handlerarg_t arg, gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                         u32 segment_size);

AMHANDLER_REGISTER(gasnetAMMessageLong);

// --------------------------------------------------------------------------------------
// function implementation
// --------------------------------------------------------------------------------------

void registerMessage(FctMessageIncoming incomingMessage) {
    fctIncomingMessage = incomingMessage;
    registerSplitMessage(incomingMessage);
}

void registerGasnetHandler() {
#define AMHANDLER_ENTRY(handler)  AMH_REGISTER_FCN(handler)();
    AMHANDLER_ENTRY(gasnetAMMessageLong);
#undef AMHANDLER_ENTRY
    gasnetSplitInit();
}

/**
 * @brief Function to be invoked for long-size message
 * A long message communication is not as efficient as the medium one.
 * we need to avoid as much as possible using a long message.
 * The future version of Gasnet should support asynchrony AM Long.
 */
void gasnetAMMessageLong(gasnet_token_t token, void *buf, size_t nbytes, gasnet_handlerarg_t arg,
                         gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                         u32 segment_size) {
    ocrCommPlatformGasnet_t *platform = getCommPlatform();
    fctIncomingMessage(platform, (ocrPolicyMsg_t *)buf, nbytes, addr_hi, addr_lo, segment_size);
}

/*
 * @brief sending long message
 *
 * we have 3 cases:
 *  (1) the target buffer is big enough to store the message
 *  (2) the target buffer is too small, we need to split the message
 *  (3) the message is too big for long message
 */
void gasnetSendLongMessage(int targetRank, ocrPolicyMsg_t * message,
                           u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                           gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                           u32 segment_size) {
    if (block != NULL) {
        void *address = block->addr;
        if (block->size > bufferSize) {
            // case 1 : the destination buffer is big enough for the message
            GASNET_Safe(gasnet_AMRequestLong4(targetRank, AMHANDLER(gasnetAMMessageLong), message, bufferSize,
                                               address, gasnetId, addr_hi, addr_lo, segment_size));
        } else {
            // ----------------------------------------------------------------------------
            // case 2: the destination buffer is not big enough, needs to split the message
            //     since gasnet AM long does not guarantee the writing of buffer is
            //     synchronized with the handler invocation, we need to wait until
            //     the partner's handler is ready to receive the next package
            // ----------------------------------------------------------------------------
            gasnetSplitToLong(targetRank, message, bufferSize, gasnetId, block,
                              addr_hi, addr_lo, segment_size);
        }
    } else {
        // case 3: no destination buffer available, use am medium to send message
        gasnetSplitToMedium(targetRank, message, bufferSize, gasnetId, block, addr_hi, addr_lo, segment_size);
    }
}

void gasnetSendHugeMessage(int targetRank, ocrPolicyMsg_t * message,
                           u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                           gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                           u32 segment_size) {
    gasnetSplitToLong(targetRank, message, bufferSize, gasnetId, block, addr_hi, addr_lo, segment_size);
}

#endif /* COMM_PLATFORM_GASNET_AMLONG */
#endif /* ENABLE_COMM_PLATFORM_GASNET */
