/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_COMM_WORKER_H__
#define __HC_COMM_WORKER_H__

#include "ocr-config.h"
#ifdef ENABLE_WORKER_HC_COMM

#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/list.h"
#include "ocr-worker.h"

typedef struct {
    ocrWorkerFactoryHc_t base;
    void (*baseInitialize) (struct _ocrWorkerFactory_t * factory,
                                  struct _ocrWorker_t *self, ocrParamList_t *perInstance);
    u8 (*baseSwitchRunlevel)(struct _ocrWorker_t* self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                     phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64), u64 val);
} ocrWorkerFactoryHcComm_t;

typedef struct {
    ocrWorkerHc_t worker;
    // cached base function pointers
    u8 (*baseSwitchRunlevel)(struct _ocrWorker_t* self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                     phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64), u64 val);
    ocrGuid_t processRequestTemplate;
    bool flushOutgoingComm;
} ocrWorkerHcComm_t;

ocrWorkerFactory_t* newOcrWorkerFactoryHcComm(ocrParamList_t *perType);

#endif /* ENABLE_WORKER_HC_COMM */
#endif /* __HC_COMM_WORKER_H__ */
