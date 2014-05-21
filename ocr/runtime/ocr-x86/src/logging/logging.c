#include "ocr-config.h"
#include "logging.h"

#include <stdio.h>
#include <time.h>

#ifdef OCR_ENABLE_LOGGING

u64 timeNs()
{
    struct timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return ts.tv_sec * 1000000000UL + (u64) ts.tv_nsec;
}

void logEDT_CREATE(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrTask_t *dst)
{
    ocrGuid_t srcGuid = src ? src->guid : 0;
    fprintf(pd->logs[DEP_LOG],
        "<EdtCreate src=\"%lu\" dst=\"%lu\" template=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        srcGuid, dst->guid, dst->templateGuid, pd->fguid.guid, timeNs());

}

void logEDT_DESTROY(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrTask_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EdtDestroy src=\"%lu\" dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        src->guid, dst->guid, pd->fguid.guid, timeNs());
}

void logEDT_SCHED(ocrPolicyDomain_t *pd, ocrTask_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EdtSched dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        dst->guid, pd->fguid.guid, timeNs());
}

void logEDT_START(ocrPolicyDomain_t *pd, ocrWorker_t *worker, ocrTask_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EdtStart dst=\"%lu\" worker=\"%lu\" funcptr=\"%s\" pd=\"%lu\" time=\"%lu\" /> \n",
        dst->guid, worker->fguid.guid, dst->name, pd->fguid.guid, timeNs());
}

void logEDT_END(ocrPolicyDomain_t *pd, ocrWorker_t *worker, ocrTask_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EdtEnd dst=\"%lu\" worker=\"%lu\" funcptr=\"%s\" pd=\"%lu\" time=\"%lu\" /> \n",
        dst->guid, worker->fguid.guid, dst->name, pd->fguid.guid, timeNs());
}

void logEVT_CREATE(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrEvent_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EventCreate src=\"%lu\" dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        src->guid, dst->guid, pd->fguid.guid, timeNs());
}

void logEVT_DESTROY(ocrPolicyDomain_t *pd, ocrTask_t *src, ocrEvent_t *dst)
{
    fprintf(pd->logs[DEP_LOG],
        "<EventDestroy src=\"%lu\" dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        src->guid, dst->guid, pd->fguid.guid, timeNs());
}

void logDEP_SATISFY(ocrPolicyDomain_t *pd, ocrGuid_t src, ocrGuid_t dst, u32 slot)
{
    fprintf(pd->logs[DEP_LOG],
        "<Satisfy src=\"%lu\" dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
        src, dst, pd->fguid.guid, timeNs());
}

void logDEP_ADD(ocrPolicyDomain_t *pd, ocrGuid_t src, ocrGuid_t dst) {
    fprintf(pd->logs[DEP_LOG],
            "<DepAdd src=\"%lu\" dst=\"%lu\" pd=\"%lu\" time=\"%lu\" /> \n",
            src, dst, pd->fguid.guid, timeNs());
}
#endif
