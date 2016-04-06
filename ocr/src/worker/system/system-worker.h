 /*
  * This file is subject to the license agreement located in the file LICENSE
  * and cannot be distributed without it. This notice cannot be
  * removed or modified.
  */

#ifndef __SYSTEM_WORKER_H__
#define __SYSTEM_WORKER_H__

#include "ocr-config.h"
#ifdef ENABLE_WORKER_SYSTEM

#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-worker.h"


typedef struct {
    ocrWorkerFactoryHc_t base;
    void (*baseInitialize) (struct _ocrWorkerFactory_t * factory,
                            struct _ocrWorker_t *self, ocrParamList_t *perInstance);

    u8 (*baseSwitchRunlevel)(struct _ocrWorker_t *self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                            phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64),
                            u64 val);

} ocrWorkerFactorySystem_t;

typedef struct {
    ocrWorkerHc_t worker;
    u64 id;
    bool readyForShutdown;

    u8 (*baseSwitchRunlevel)(struct _ocrWorker_t *self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                             phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64),
                             u64 val);

}ocrWorkerSystem_t;

ocrWorkerFactory_t* newOcrWorkerFactorySystem(ocrParamList_t *perType);

#endif /* ENABLE_WORKER_SYSTEM */
#endif /* __SYSTEM_WORKER_H__ */
