/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 *
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HEURISTIC_NULL

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-workpile.h"
#include "ocr-scheduler-object.h"
#include "scheduler-heuristic/null/null-scheduler-heuristic.h"

/******************************************************/
/* OCR-NULL SCHEDULER_HEURISTIC                                 */
/******************************************************/

ocrSchedulerHeuristic_t* newSchedulerHeuristicNull(ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance) {
    ocrSchedulerHeuristic_t* self = (ocrSchedulerHeuristic_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHeuristicNull_t), PERSISTENT_CHUNK);
    initializeSchedulerHeuristicOcr(factory, self, perInstance);
    return self;
}

void nullSchedulerHeuristicBegin(ocrSchedulerHeuristic_t * self) {
    ASSERT(self->scheduler != NULL);
}

void nullSchedulerHeuristicStart(ocrSchedulerHeuristic_t * self) {
    // Nothing to do locally
}

void nullSchedulerHeuristicStop(ocrSchedulerHeuristic_t * self) {
    // Nothing to do locally
}

void nullSchedulerHeuristicFinish(ocrSchedulerHeuristic_t *self) {
    // Nothing to do locally
}

void nullSchedulerHeuristicDestruct(ocrSchedulerHeuristic_t * self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

u8 nullSchedulerHeuristicUpdate(ocrSchedulerHeuristic_t *self, ocrSchedulerObject_t *schedulerObject, u32 properties) {
    return OCR_ENOTSUP;
}

ocrSchedulerHeuristicContext_t* nullSchedulerHeuristicGetContext(ocrSchedulerHeuristic_t *self, u64 contextId) {
    ASSERT(0);
    return NULL;
}

u8 nullSchedulerHeuristicRegisterContext(ocrSchedulerHeuristic_t *self, u64 contextId, ocrLocation_t loc) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerHeuristicGiveInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerHeuristicTakeInvoke(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerHeuristicGiveSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

u8 nullSchedulerHeuristicTakeSimulate(ocrSchedulerHeuristic_t *self, ocrSchedulerHeuristicContext_t *context, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints) {
    return OCR_ENOTSUP;
}

/******************************************************/
/* OCR-NULL SCHEDULER_HEURISTIC FACTORY                         */
/******************************************************/

void destructSchedulerHeuristicFactoryNull(ocrSchedulerHeuristicFactory_t * factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrSchedulerHeuristicFactory_t * newOcrSchedulerHeuristicFactoryNull(ocrParamList_t *perType) {
    ocrSchedulerHeuristicFactory_t* base = (ocrSchedulerHeuristicFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerHeuristicFactoryNull_t), NONPERSISTENT_CHUNK);
    base->instantiate = &newSchedulerHeuristicNull;
    base->destruct = &destructSchedulerHeuristicFactoryNull;
    base->fcts.begin = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), nullSchedulerHeuristicBegin);
    base->fcts.start = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), nullSchedulerHeuristicStart);
    base->fcts.stop = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), nullSchedulerHeuristicStop);
    base->fcts.finish = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), nullSchedulerHeuristicFinish);
    base->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerHeuristic_t*), nullSchedulerHeuristicDestruct);

    base->fcts.update = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u32), nullSchedulerHeuristicUpdate);
    base->fcts.getContext = FUNC_ADDR(ocrSchedulerHeuristicContext_t* (*)(ocrSchedulerHeuristic_t*, u64), nullSchedulerHeuristicGetContext);
    base->fcts.registerContext = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, u64, ocrLocation_t), nullSchedulerHeuristicRegisterContext);

    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GIVE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), nullSchedulerHeuristicGiveInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TAKE].invoke = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), nullSchedulerHeuristicTakeInvoke);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_GIVE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), nullSchedulerHeuristicGiveSimulate);
    base->fcts.op[OCR_SCHEDULER_HEURISTIC_OP_TAKE].simulate = FUNC_ADDR(u8 (*)(ocrSchedulerHeuristic_t*, ocrSchedulerHeuristicContext_t*, ocrSchedulerOpArgs_t*, ocrRuntimeHint_t*), nullSchedulerHeuristicTakeSimulate);
    return base;
}

#endif /* ENABLE_SCHEDULER_HEURISTIC_NULL */
