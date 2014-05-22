/*
 * This file is subject to the license agreement located in the file LIXENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_XE

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "scheduler/xe/xe-scheduler.h"

/******************************************************/
/* OCR-XE SCHEDULER                                   */
/******************************************************/

void xeSchedulerDestruct(ocrScheduler_t * self) {
}

void xeSchedulerBegin(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
}

void xeSchedulerStart(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
}

void xeSchedulerStop(ocrScheduler_t * self) {
}

void xeSchedulerFinish(ocrScheduler_t *self) {
}

u8 xeSchedulerTake(ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerGive(ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    return OCR_ENOTSUP;
}

ocrScheduler_t* newSchedulerXe(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* derived = (ocrScheduler_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerXe_t), NULL);
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeSchedulerXe(ocrSchedulerFactory_t * factory, ocrScheduler_t * derived, ocrParamList_t * perInstance){
    initializeSchedulerOcr(factory, derived, perInstance);
}

void destructSchedulerFactoryXe(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryXe(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryXe_t), NULL);

    base->instantiate = &newSchedulerXe;
    base->initialize  = &initializeSchedulerXe;
    base->destruct = &destructSchedulerFactoryXe;

    base->schedulerFcts.begin = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), xeSchedulerBegin);
    base->schedulerFcts.start = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), xeSchedulerStart);
    base->schedulerFcts.stop = FUNC_ADDR(void (*)(ocrScheduler_t*), xeSchedulerStop);
    base->schedulerFcts.finish = FUNC_ADDR(void (*)(ocrScheduler_t*), xeSchedulerFinish);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), xeSchedulerDestruct);
    base->schedulerFcts.take = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), xeSchedulerTake);
    base->schedulerFcts.give = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), xeSchedulerGive);
    return base;
}

#endif /* ENABLE_SCHEDULER_XE */
