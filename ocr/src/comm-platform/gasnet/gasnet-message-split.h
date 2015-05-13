
#ifndef __GASNET_MESSAGE_SPLIT_H__
#define __GASNET_MESSAGE_SPLIT_H__

#include "ocr-policy-domain.h"
#include "gasnet-comm-platform.h"
#include "gasnet-share-segment.h"

void gasnetSplitInit();
void registerSplitMessage(FctMessageIncoming incomingMessage);

/*
 * @brief partition a long message into medium messages
 */
void gasnetSplitToMedium( int targetRank, ocrPolicyMsg_t * message,
                  u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                  gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                  u32 segment_size);

/*
 * @brief partition a long message into medium messages
 */
void gasnetSplitToLong( int targetRank, ocrPolicyMsg_t * message,
                  u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                  gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                  u32 segment_size);

#endif
