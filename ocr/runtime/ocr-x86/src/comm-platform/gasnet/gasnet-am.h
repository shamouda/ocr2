
#ifndef __GASNET_AM_H__
#define __GASNET_AM_H__

#include "gasnet-share-segment.h" // for gasnetCommBlock_t

//---------------------------------------------------------------------------
// Callback type
//---------------------------------------------------------------------------
typedef void (*FctMessageIncoming)(ocrCommPlatformGasnet_t *platform ,
                                   ocrPolicyMsg_t *msg, uint64_t size,
                                   gasnet_handlerarg_t seg_addr_hi, gasnet_handlerarg_t seg_addr_lo,
                                   gasnet_handlerarg_t seg_size);


//---------------------------------------------------------------------------
// The following functions have to be implemented by gasnet-ammedium.c and
// gasnet-amlong.c
// ocr-config.h will choose either one of the file to be included
//---------------------------------------------------------------------------

// ========================================================================================
// WARNING ! Treatement for a long message:
//   We have two ways to treat a long message:
//   (1) using GASNet's AM long message with possibility of races if two or more processes
//       send at the same target node with overlap target address.
//   (2) using GASNET's AM medium message by decoupling the long message into several
//       medium messages. This one is safer with some performance penalties
// ========================================================================================

void gasnetSendLongMessage(int targetRank, ocrPolicyMsg_t * message,
                           u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                           gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                           u32 segment_size);

void gasnetSendHugeMessage(int targetRank, ocrPolicyMsg_t * message,
                           u64 bufferSize, u64 gasnetId, gasnetCommBlock_t *block,
                           gasnet_handlerarg_t addr_hi, gasnet_handlerarg_t addr_lo,
                           u32 segment_size);

void registerMessage( FctMessageIncoming incomingMessage );

void registerGasnetHandler();

#endif // __GASNET_AM_H__
