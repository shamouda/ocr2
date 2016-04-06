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
#include "utils/comQueue.h"

typedef struct {
    ocrLocation_t neighborLocation;  /**< Location of the neigbhor */
    comQueue_t *outQueue;            /**< Queue to send to for this neighbor */
} neighborQueue_t;

typedef struct {
    ocrCommPlatformFactory_t base;
} ocrCommPlatformFactoryCePthread_t;

/* Note that we have two incoming queues because otherwise
 * a livelock could happen if two CEs were trying to get work from
 * one another due to an XE asking for work. That XE request would
 * block the inQueueXE (potentially) and a livelock would ensue
 */
typedef struct {
    ocrCommPlatform_t base;
    comQueue_t inQueueXE;        /**< Incoming queue for XEs (the one I poll) */
    comQueue_t inQueueCE;        /**< Incoming queue for CEs (the one I poll) */
    u32 curPollCount;            /**< Current poll count */
    u32 numOutQueues;            /**< Number of outgoing queues */
    neighborQueue_t *outQueues;  /**< Outgoing queues for my location */
} ocrCommPlatformCePthread_t;

typedef struct {
    paramListCommPlatformInst_t base;
} paramListCommPlatformCePthread_t;

extern ocrCommPlatformFactory_t* newCommPlatformFactoryCePthread(ocrParamList_t *perType);

#endif /* ENABLE_COMM_PLATFORM_CE_PTHREAD */
#endif /* __COMM_PLATFORM_CE_PTHREAD_H__ */
