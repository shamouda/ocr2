/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_HC

#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-cost-mapper.h"
#include "scheduler/hc/hc-scheduler.h"

/******************************************************/
/* OCR-HC SCHEDULER                                   */
/******************************************************/

void hcSchedulerDestruct(ocrScheduler_t * self) {
    u64 i;
    // Destruct the mappers
    u64 count = self->mapperCount;
    for(i = 0; i < count; ++i) {
        self->mappers[i]->fcts.destruct(self->mappers[i]);
    }
    runtimeChunkFree((u64)(self->mappers), NULL);
    
    runtimeChunkFree((u64)self, NULL);
}

void hcSchedulerBegin(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
    // Nothing to do locally
    u64 mapperCount = self->mapperCount;
    u64 i;
    for(i = 0; i < mapperCount; ++i) {
        self->mappers[i]->fcts.begin(self->mappers[i], PD);
    }
}

void hcSchedulerStart(ocrScheduler_t * self, ocrPolicyDomain_t * PD) {
    // Get a GUID
    guidify(PD, (u64)self, &(self->fguid), OCR_GUID_SCHEDULER);
    self->pd = PD;
    
    u64 mapperCount = self->mapperCount;
    u64 i;
    for(i = 0; i < mapperCount; ++i) {
        self->mappers[i]->fcts.start(self->mappers[i], PD);
    }
}

void hcSchedulerStop(ocrScheduler_t * self) {
    ocrPolicyDomain_t *pd = NULL;
    ocrPolicyMsg_t msg;
    getCurrentEnv(&pd, NULL, NULL, &msg);
    
    // Stop the mappers
    u64 i = 0;
    u64 count = self->mapperCount;
    for(i = 0; i < count; ++i) {
        self->mappers[i]->fcts.stop(self->mappers[i]);
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

void hcSchedulerFinish(ocrScheduler_t *self) {
    u64 i = 0;
    u64 count = self->mapperCount;
    for(i = 0; i < count; ++i) {
        self->mappers[i]->fcts.finish(self->mappers[i]);
    }
}

u8 hcSchedulerTake(ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties) {
    self->mappers[0]->fcts.take(self->mappers[0], source, component, hints, NULL, NULL, properties);
    return 0;
}

u8 hcSchedulerGive(ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties) {
    self->mappers[0]->fcts.give(self->mappers[0], source, component, hints, properties);
    return 0;
}

ocrScheduler_t* newSchedulerHc(ocrSchedulerFactory_t * factory, ocrParamList_t *perInstance) {
    ocrScheduler_t* base = (ocrScheduler_t*) runtimeChunkAlloc(sizeof(ocrSchedulerHc_t), NULL);
    factory->initialize(factory, base, perInstance);
    return base;
}

void initializeSchedulerHc(ocrSchedulerFactory_t * factory, ocrScheduler_t *self, ocrParamList_t *perInstance) {
    initializeSchedulerOcr(factory, self, perInstance);
}

void destructSchedulerFactoryHc(ocrSchedulerFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrSchedulerFactory_t * newOcrSchedulerFactoryHc(ocrParamList_t *perType) {
    ocrSchedulerFactory_t* base = (ocrSchedulerFactory_t*) runtimeChunkAlloc(
        sizeof(ocrSchedulerFactoryHc_t), (void *)1);
    
    base->instantiate = &newSchedulerHc;
    base->initialize  = &initializeSchedulerHc;
    base->destruct = &destructSchedulerFactoryHc;

    base->schedulerFcts.begin = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), hcSchedulerBegin);
    base->schedulerFcts.start = FUNC_ADDR(void (*)(ocrScheduler_t*, ocrPolicyDomain_t*), hcSchedulerStart);
    base->schedulerFcts.stop = FUNC_ADDR(void (*)(ocrScheduler_t*), hcSchedulerStop);
    base->schedulerFcts.finish = FUNC_ADDR(void (*)(ocrScheduler_t*), hcSchedulerFinish);
    base->schedulerFcts.destruct = FUNC_ADDR(void (*)(ocrScheduler_t*), hcSchedulerDestruct);
    base->schedulerFcts.take = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrFatGuid_t*, ocrFatGuid_t, u32), hcSchedulerTake);
    base->schedulerFcts.give = FUNC_ADDR(u8 (*)(ocrScheduler_t*, ocrLocation_t, ocrFatGuid_t, ocrFatGuid_t, u32), hcSchedulerGive);

    return base;
}


#endif /* ENABLE_SCHEDULER_HC */
