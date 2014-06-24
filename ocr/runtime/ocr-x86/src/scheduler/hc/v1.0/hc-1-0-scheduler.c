/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HC_1_0

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-cost-mapper.h"
#include "scheduler/hc/hc-scheduler.h"

/******************************************************/
/* OCR-CE-1-0 SCHEDULER                               */
/******************************************************/

void hc_1_0_SchedulerDestruct(ocrScheduler_t * self) {
    u64 i;
    // Destruct the mappers
    u64 count = self->costmapperCount;
    for(i = 0; i < count; ++i) {
        self->costmappers[i]->fcts.destruct(self->costmappers[i]);
    }
    runtimeChunkFree((u64)(self->costmappers), NULL);

    runtimeChunkFree((u64)self, NULL);
}

void hc_1_0_SchedulerBegin(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
    // Nothing to do locally
    u64 costmapperCount = self->costmapperCount;
    u64 i;
    for(i = 0; i < costmapperCount; ++i) {
        self->costmappers[i]->fcts.begin(self->costmappers[i], PD);
    }
}

void hc_1_0_SchedulerStart(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
    // Get a GUID
    guidify(PD, (u64)self, &(self->fguid), OCR_GUID_SCHEDULER);
    self->pd = PD;

    u64 costmapperCount = self->costmapperCount;
    u64 i;
    for(i = 0; i < costmapperCount; ++i) {
        self->costmappers[i]->fcts.start(self->costmappers[i], PD);
    }
}

void hc_1_0_SchedulerStop(ocrScheduler_t * self) {
    ocrPolicyDomain_t *pd = NULL;
    ocrPolicyMsg_t msg;
    getCurrentEnv(&pd, NULL, NULL, &msg);

    // Stop the mappers
    u64 i = 0;
    u64 count = self->costmapperCount;
    for(i = 0; i < count; ++i) {
        self->costmappers[i]->fcts.stop(self->costmappers[i]);
    }

    // Destroy the GUID

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
    msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
    PD_MSG_FIELD(guid) = self->fguid;
    PD_MSG_FIELD(guid.metaDataPtr) = self;
    PD_MSG_FIELD(properties) = 0;
    // Ignore failure, probably shutting down
    pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    self->fguid.guid = UNINITIALIZED_GUID;
}

void hc_1_0_SchedulerFinish(ocrScheduler_t *self) {
    u64 i = 0;
    u64 count = self->costmapperCount;
    for(i = 0; i < count; ++i) {
        self->costmappers[i]->fcts.finish(self->costmappers[i]);
    }
    // Nothing to do locally
}

u8 hc_1_0_SchedulerTake(ocrScheduler_t *self, ocrLocation_t source, ocrGuid_t *component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    return self->costmappers[0]->fcts.take(self->costmappers[0], source, component, kind, hints, NULL, NULL, properties);
}

u8 hc_1_0_SchedulerGive(ocrScheduler_t *self, ocrLocation_t source, ocrGuid_t component, ocrGuidKind kind, ocrParamListHint_t *hints, u32 properties) {
    ASSERT(properties == SCHED_PROP_READY);
    return self->costmappers[0]->fcts.give(self->costmappers[0], source, component, kind, hints, properties);
}

void hc_1_0_SchedulerUpdate(ocrScheduler_t *self, u32 properties) {
}

ocrScheduler_t* newSchedulerHc(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* base = (ocrScheduler_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerHc_t), NULL);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeSchedulerHc(ocrSchedulerFactory_t *factory, ocrScheduler_t *self, ocrParamList_t *perInstance) {
    initializeSchedulerOcr(factory, self, perInstance);
}

void destructSchedulerFactoryHc(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryHc(ocrParamList_t *perType) {
    ocrSchedulerFactoryHc_t* derived = (ocrSchedulerFactoryHc_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryHc_t), NULL);

    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) derived;
    base->instantiate = &newSchedulerHc;
    base->initialize  = &initializeSchedulerHc;
    base->destruct = &destructSchedulerFactoryHc;
    base->schedulerFcts.begin = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), hc_1_0_SchedulerBegin);
    base->schedulerFcts.start = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), hc_1_0_SchedulerStart);
    base->schedulerFcts.stop = FUNC_ADDR(void (*)(ocrScheduler_t*), hc_1_0_SchedulerStop);
    base->schedulerFcts.finish = FUNC_ADDR(void (*)(ocrScheduler_t*), hc_1_0_SchedulerFinish);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), hc_1_0_SchedulerDestruct);
    base->schedulerFcts.take = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrGuid_t*, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_SchedulerTake);
    base->schedulerFcts.give = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrGuid_t, ocrGuidKind, ocrParamListHint_t*, u32), hc_1_0_SchedulerGive);
    base->schedulerFcts.update = FUNC_ADDR(void (*)(ocrScheduler_t*, u32), hc_1_0_SchedulerUpdate);

    return base;
}

#endif /* ENABLE_SCHEDULER_HC_1_0 */
