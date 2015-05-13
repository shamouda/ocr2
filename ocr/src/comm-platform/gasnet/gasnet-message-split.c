
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_COMM_PLATFORM_GASNET

#include <assert.h>
#include <gasnet.h>

#include "debug.h"  // includes DPRINTF

#include "gasnet-comm-platform.h"
#include "gasnet-policy-domain.h"
#include "amhandler.h"
#include "gasnet-message.h"
#include "gasnet-share-segment.h"


// --------------------------------------------------------------------------------------
// macros
// --------------------------------------------------------------------------------------

// message flag: 0 means just sent, otherwise the partner is ready
#define MESSAGE_WAIT    '0'
#define MESSAGE_READY   '1'

typedef void (*FctMessageIncoming)(ocrCommPlatformGasnet_t *platform ,
                                   ocrPolicyMsg_t *msg, uint64_t size,
                                   gasnet_handlerarg_t seg_addr_hi, gasnet_handlerarg_t seg_addr_lo,
                                   gasnet_handlerarg_t seg_size);

// --------------------------------------------------------------------------------------
// variables
// --------------------------------------------------------------------------------------
static FctMessageIncoming fctIncomingMessage;


// --------------------------------------------------------------------------------------
// prototypes declaration
// --------------------------------------------------------------------------------------

static void
gasnetAMMessageMediumHelper(gasnet_token_t token, void *buf, size_t nbytes,
                    gasnet_handlerarg_t messageID, gasnet_handlerarg_t position,
                    gasnet_handlerarg_t total_partitions, gasnet_handlerarg_t total_size);

static void gasnetAMMessageLongHelper(gasnet_token_t token, void *buf, size_t nbytes,
        gasnet_handlerarg_t arg, gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo, u32 segment_size,
    gasnet_handlerarg_t position, gasnet_handlerarg_t total_partitions, gasnet_handlerarg_t total_size,
    gasnet_handlerarg_t reply_hi, gasnet_handlerarg_t reply_lo);

static void gasnetAMMessageLongReply(gasnet_token_t token,
    gasnet_handlerarg_t reply_hi, gasnet_handlerarg_t reply_lo);


AMHANDLER_REGISTER(gasnetAMMessageLongHelper);
AMHANDLER_REGISTER(gasnetAMMessageLongReply);
AMHANDLER_REGISTER(gasnetAMMessageMediumHelper);

// --------------------------------------------------------------------------------------
// Static Implementation
// --------------------------------------------------------------------------------------

/*
 * @brief Reply handler for long message
 *
 * This function will change the value of the flag to other than '0'
 */
static void gasnetAMMessageLongReply(gasnet_token_t token,
                                     gasnet_handlerarg_t reply_hi, gasnet_handlerarg_t reply_lo) {
    char* reply = (char*) getBits64(reply_hi, reply_lo);
    *reply = MESSAGE_READY;
}

/*
 * @brief Helper handler function for long message
 *
 * This handler is invoked when the sender needs to split a long message and
 * send it with multiple packages
 *
 * The receiver is then store the package into its database and notify to sender
 * that it's ready for another package
 */
static void gasnetAMMessageLongHelper(gasnet_token_t token, void *buf, size_t nbytes,
    gasnet_handlerarg_t messageID, gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo, u32 segment_size,
    gasnet_handlerarg_t position, gasnet_handlerarg_t total_partitions, gasnet_handlerarg_t total_size,
    gasnet_handlerarg_t reply_hi, gasnet_handlerarg_t reply_lo) {
    ocrCommPlatformGasnet_t *platform = getCommPlatform();

    // protect pushing data into the tree
    // We may need mutex to avoid potential race condition

    void *message = gasnet_message_push(platform->base.pd, (int) messageID, buf, nbytes, (int) position,
    (int)total_partitions, (int)total_size );

    // end of mutex region

    if (message != NULL) {
        fctIncomingMessage(platform, (ocrPolicyMsg_t *)message, total_size,
                           (gasnet_handlerarg_t) addr_hi, (gasnet_handlerarg_t) addr_lo,
                           (gasnet_handlerarg_t) segment_size);
        gasnet_message_pop( platform->base.pd, messageID );
    }
    // notify to sender that we are ready to receive another package
    gasnet_AMReplyShort2(token, AMHANDLER(gasnetAMMessageLongReply), reply_hi, reply_lo);
}

/*
 * @brief active message function for medium message
 *
 * @param targetRank   : the target node
 * @param message      : the message
 * @param bufferSize   : the size of the message (in bytes)
 * @param gasnetId     : ID of the message
 * @param position     : the positon of this message from the big message
 * @param max_medium_size : the maximum size of the medium message
 */
static void gasnetAMMessageMediumHelper(gasnet_token_t token, void *buf, size_t nbytes,
        gasnet_handlerarg_t messageID, gasnet_handlerarg_t position,
        gasnet_handlerarg_t total_partitions, gasnet_handlerarg_t total_size)
{
    ocrCommPlatformGasnet_t *platform = getCommPlatform();

    void *message = gasnet_message_push(platform->base.pd, (int) messageID, buf, nbytes,
                                        (int) position, (int)total_partitions, (int)total_size );
    if (message != NULL) {
        fctIncomingMessage(platform, (ocrPolicyMsg_t *)message, total_size,
                          (gasnet_handlerarg_t)0, (gasnet_handlerarg_t)0, (gasnet_handlerarg_t)0);
        gasnet_message_pop( platform->base.pd, messageID );
    }
}


//---------------------------------------------------------------------------------------------------
// INTERFACES
//---------------------------------------------------------------------------------------------------

void registerSplitMessage(FctMessageIncoming incomingMessage) {
    fctIncomingMessage = incomingMessage;
}

void gasnetSplitInit() {
#define AMHANDLER_ENTRY(handler)  AMH_REGISTER_FCN(handler)();
    AMHANDLER_ENTRY(gasnetAMMessageLongHelper);
    AMHANDLER_ENTRY(gasnetAMMessageLongReply);
    AMHANDLER_ENTRY(gasnetAMMessageMediumHelper);
#undef AMHANDLER_ENTRY
}

/*
 * @brief partition a long message into medium messages
 *
 * @param targetRank   : the target node
 * @param message      : the message
 * @param bufferSize   : the size of the message (in bytes
 * @param gasnetId     : ID of the message
 * @param position     : the positon of this message from the big message
 * @param max_medium_size : the maximum size of the medium message
 */
void gasnetSplitToMedium( int targetRank, ocrPolicyMsg_t * message,
                          u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                          gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                          u32 segment_size) {
   // use AM's medium request by partitioning the message
    const int max_medium_size = gasnet_AMMaxMedium();
    int i, position=0, size, tot_size_so_far=0;
    const unsigned int num_stages = (bufferSize / max_medium_size) + ( bufferSize%max_medium_size== 0? 0: 1);

    for ( i=0; i<num_stages; i++) {
        size = (i+1 < num_stages ? max_medium_size : bufferSize - tot_size_so_far);
        GASNET_Safe( gasnet_AMRequestMedium4(targetRank, AMHANDLER(gasnetAMMessageMediumHelper),
                                             (void*)message+position,(size_t) size,
                                             (gasnet_handlerarg_t) gasnetId, (gasnet_handlerarg_t) position,
                                             (gasnet_handlerarg_t) num_stages, (gasnet_handlerarg_t) bufferSize));
        position += max_medium_size;
        tot_size_so_far += size;
    }
}

/*
 * @brief Partition a long message into medium messages
 */
void gasnetSplitToLong(int targetRank, ocrPolicyMsg_t * message,
                  u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                  gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                  u32 segment_size) {
    // the destination buffer is not big enough, needs to split the message
    // since gasnet AM long does not guarantee the writing of buffer is
    // synchronized with the handler invocation, we need to wait until
    // the partner's handler is ready to receive the next package

    volatile char partner_reply = MESSAGE_WAIT;
    gasnet_handlerarg_t reply_hi = (gasnet_handlerarg_t) BITS64_HIGH(&partner_reply);
    gasnet_handlerarg_t reply_lo = (gasnet_handlerarg_t) BITS64_LOW (&partner_reply);

    int i, position=0, size, tot_size_so_far=0;
    const unsigned int num_stages = (bufferSize / block->size) + ( bufferSize % block->size == 0? 0: 1);

    for (i=0; i<num_stages; i++) {
        partner_reply = MESSAGE_WAIT;

        size = ( i+1 < num_stages? block->size: bufferSize - tot_size_so_far);

        // sending to the target rank the i-th portion of the message, and wait the reply
        // from the target node.

        gasnet_AMRequestLong9(targetRank, AMHANDLER(gasnetAMMessageLongHelper),
                              (void*)message+position, (size_t) size, block->addr,
                              (gasnet_handlerarg_t) gasnetId, addr_hi, addr_lo, segment_size,
                              (gasnet_handlerarg_t) position,
                              (gasnet_handlerarg_t) num_stages, (gasnet_handlerarg_t) bufferSize,
                              (gasnet_handlerarg_t) reply_hi, (gasnet_handlerarg_t) reply_lo);
        GASNET_BLOCKUNTIL(partner_reply != '0');
        // prepating for the next package
        position += block->size;
        tot_size_so_far += size;
    }
}

#endif // ENABLE_COMM_PLATFORM_GASNET
