#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_GASNET
#ifndef COMM_PLATFORM_GASNET_AMLONG

#include "ocr-policy-domain.h"
#include "utils/ocr-utils.h"

#include "debug.h"

#include "amhandler.h"
#include "gasnet-comm-platform.h"
#include "gasnet-policy-domain.h"
#include "gasnet-message.h"
#include "gasnet-am.h"
#include "gasnet-message-split.h"

#include <gasnet.h>

// --------------------------------------------------------------------------------------
// function implementation
// --------------------------------------------------------------------------------------

/******
 * register am handler (called by external module such as gasnet-comm-platform.c)
 ******/
void registerGasnetHandler() {
    gasnetSplitInit();
}

/*****
 * register a incoming message callback. The function incommingMessage will be called
 * when a message has arrived. The caller needs to implement this function.
 *****/
void registerMessage(FctMessageIncoming incomingMessage) {
  registerSplitMessage(incomingMessage);
}


/*
 * @brief Partitions a long message into medium messages
 */
void gasnetSendLongMessage(int targetRank, ocrPolicyMsg_t * message,
                           u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                           gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                           u32 segment_size) {
   gasnetSplitToMedium(targetRank, message, bufferSize, gasnetId, block,
                       addr_hi, addr_lo, segment_size );
}


/*
 * @brief Partitions a huge message into medium messages
 */

void gasnetSendHugeMessage( int targetRank, ocrPolicyMsg_t * message,
                  u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                  gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                  u32 segment_size) {
   gasnetSendLongMessage(targetRank, message, bufferSize, gasnetId, block,
                         addr_hi, addr_lo, segment_size );
}

#endif
#endif
