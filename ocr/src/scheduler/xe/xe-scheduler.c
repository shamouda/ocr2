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

u8 xeSchedulerSwitchRunlevel(ocrScheduler_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                             phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {
    return 0;
}

u8 xeSchedulerTake(ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts) {
    return 0;
}

u8 xeSchedulerGive(ocrScheduler_t* base, u32* count, ocrFatGuid_t* edts) {
    return 0;
}

u8 xeSchedulerTakeComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return OCR_ENOSYS;
}

u8 xeSchedulerGiveComm(ocrScheduler_t *self, u32* count, ocrFatGuid_t* handlers, u32 properties) {
    return OCR_ENOSYS;
}

///////////////////////////////
//      Scheduler 1.0        //
///////////////////////////////

u8 xeSchedulerGetWorkInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerNotifyInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerTransactInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerAnalyzeInvoke(ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 xeSchedulerUpdate(ocrScheduler_t *self, u32 properties) {
    return OCR_ENOTSUP;
}

ocrScheduler_t* newSchedulerXe(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* derived = (ocrScheduler_t*) runtimeChunkAlloc(
                                  sizeof(ocrSchedulerXe_t), PERSISTENT_CHUNK);
    factory->initialize(factory, derived, perInstance);
    return derived;
}

void initializeSchedulerXe(ocrSchedulerFactory_t * factory, ocrScheduler_t * derived, ocrParamList_t * perInstance) {
    initializeSchedulerOcr(factory, derived, perInstance);
}

void destructSchedulerFactoryXe(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryXe(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerFactoryXe_t), NONPERSISTENT_CHUNK);

    base->instantiate = &newSchedulerXe;
    base->initialize  = &initializeSchedulerXe;
    base->destruct = &destructSchedulerFactoryXe;

    base->schedulerFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                          phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), xeSchedulerSwitchRunlevel);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), xeSchedulerDestruct);
    base->schedulerFcts.takeEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), xeSchedulerTake);
    base->schedulerFcts.giveEdt = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*), xeSchedulerGive);
    base->schedulerFcts.takeComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), xeSchedulerTakeComm);
    base->schedulerFcts.giveComm = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32*, ocrFatGuid_t*, u32), xeSchedulerGiveComm);

    //Scheduler 1.0
    base->schedulerFcts.update = FUNC_ADDR(u8 (*)(ocrScheduler_t*, u32), xeSchedulerUpdate);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_GET_WORK].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerGetWorkInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_NOTIFY].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerNotifyInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_TRANSACT].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerTransactInvoke);
    base->schedulerFcts.op[OCR_SCHEDULER_OP_ANALYZE].invoke = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), xeSchedulerAnalyzeInvoke);
    return base;
}

#endif /* ENABLE_SCHEDULER_XE */
