
#ifndef __LOGGING_H__

#ifdef OCR_ENABLE_LOGGING

// TODO: not sure what to include here...
#include "debug.h"
#include "hc/hc.h"
#include "ocr-policy-domain.h"
#include "ocr-task.h"
#include "ocr-worker.h"

#define DEP_LOG 0

void logEDT_CREATE(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrTask_t *dst);
void logEDT_DESTROY(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrTask_t *dst);
void logEDT_START(ocrPolicyDomain_t *pd, ocrWorker_t *worker, ocrTask_t *dst);
void logEDT_END(ocrPolicyDomain_t *pd, ocrWorker_t *worker, ocrTask_t *dst);
void logEDT_SCHED(ocrPolicyDomain_t *pd, ocrTask_t *dst);
void logEVT_CREATE(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrEvent_t *dst);
void logEVT_DESTROY(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrEvent_t *dst);
void logDEP_SATISFY(ocrPolicyDomain_t *pd, ocrGuid_t src, ocrGuid_t dst, u32 slot);

#endif /* OCR_ENABLE_LOGGING */

#endif
