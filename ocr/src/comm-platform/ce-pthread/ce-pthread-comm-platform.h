/**
 * @brief CE_PTHREAD communication platform (no communication; only one policy domain)
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __COMM_PLATFORM_CE_PTHREAD_H__
#define __COMM_PLATFORM_CE_PTHREAD_H__

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_CE_PTHREAD

#include "ocr-comm-platform.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/deque.h"


typedef struct {
    ocrCommPlatformFactory_t base;
} ocrCommPlatformFactoryCePthread_t;

/**
 * A comm-platform maintains a local index for every neighbor
 * it communicates with. This local index is embedded into the
 * policy message and is extracted by the policy message processing
 * function. The local index can either be embedded by the sender
 * or by the comm platform of the receiver is it is not set.
 */
typedef struct _ocrNeighbor_t {
    ocrLocation_t loc;  /**< Global location ID of the neighbor */
    u64 seqId;          /**< Index of this policy domain at neighbor loc policy domain */
} ocrNeighbor_t;

typedef struct {
    volatile ocrPolicyMsg_t * message; //typically used for 2-way messages that require response
    ocrPolicyMsg_t messageBuffer;      //typically used for 1-way messages that do not require response
    ocrPolicyMsg_t overwriteBuffer;    //used for overriding messages such as shutdown
    volatile u64 remoteCounter;        //counter used by remote agent
    volatile u64 localCounter;         //counter used by channel owner
    ocrLocation_t remoteLocation;      //location of remote agent
    volatile bool msgCancel;           //cancel flag
    char padding[64];
} ocrCommChannel_t;

typedef struct {
    ocrCommPlatform_t base;
    u32 startIdx;                         //start of next poll loop (to avoid starvation)
    u32 numChannels;                      //number of channels owned by me
    u32 numNeighborChannels;              //number of channels I access as a remote agent
    ocrCommChannel_t * channels;          //array of comm channels owned by me
    ocrCommChannel_t ** neighborChannels; //comm channels in other CEs I access as a remote agent
    u32 channelIdx;                       //used to setup the channels
    ocrNeighbor_t *locationTT;            //Neighbor translation table for translating global location to local index
    u64 countTT;                          //The number entries in the translation table
    u64 parentIdx;                        //Index in locationTT for the parent
} ocrCommPlatformCePthread_t;

typedef struct {
    paramListCommPlatformInst_t base;
} paramListCommPlatformCePthread_t;

extern ocrCommPlatformFactory_t* newCommPlatformFactoryCePthread(ocrParamList_t *perType);

#endif /* ENABLE_COMM_PLATFORM_CE_PTHREAD */
#endif /* __COMM_PLATFORM_CE_PTHREAD_H__ */
