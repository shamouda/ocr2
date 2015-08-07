/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DOMAIN

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/domain/domain-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-DOMAIN SCHEDULER_OBJECT FUNCTIONS              */
/******************************************************/

static void domainSchedulerObjectInit(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD) {
    ocrSchedulerObjectDomain_t *domainSchedObj = (ocrSchedulerObjectDomain_t*)self;
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    paramListSchedulerObjectMap_t paramMap;
    paramMap.base.kind = OCR_SCHEDULER_OBJECT_MAP | OCR_SCHEDULER_OBJECT_AGGREGATE;
    paramMap.base.guidRequired = 0;
    paramMap.type = OCR_MAP_TYPE_MODULO_LOCKED;
    paramMap.nbBuckets = 16;
    ocrSchedulerObjectFactory_t *mapFactory = PD->schedulerObjectFactories[schedulerObjectMap_id];
    domainSchedObj->dbTable = mapFactory->fcts.create(mapFactory, (ocrParamList_t*)(&paramMap));
#else
    ASSERT(0);
#endif

#ifdef ENABLE_SCHEDULER_OBJECT_WST
    paramListSchedulerObjectWst_t paramWst;
    paramWst.base.kind = OCR_SCHEDULER_OBJECT_WST | OCR_SCHEDULER_OBJECT_AGGREGATE;
    paramWst.base.guidRequired = 0;
    ocrSchedulerObjectFactory_t *wstFactory = PD->schedulerObjectFactories[schedulerObjectWst_id];
    domainSchedObj->wst = wstFactory->fcts.create(wstFactory, (ocrParamList_t*)(&paramWst));
#else
    ASSERT(0);
#endif
}

static void domainSchedulerObjectFinish(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD) {
    ocrSchedulerObjectDomain_t *domainSchedObj = (ocrSchedulerObjectDomain_t*)self;
#ifdef ENABLE_SCHEDULER_OBJECT_MAP
    ocrSchedulerObjectFactory_t *mapFactory = PD->schedulerObjectFactories[schedulerObjectMap_id];
    mapFactory->fcts.destroy(mapFactory, domainSchedObj->dbTable);
#else
    ASSERT(0);
#endif

#ifdef ENABLE_SCHEDULER_OBJECT_WST
    ocrSchedulerObjectFactory_t *wstFactory = PD->schedulerObjectFactories[schedulerObjectWst_id];
    wstFactory->fcts.destroy(wstFactory, domainSchedObj->wst);
#else
    ASSERT(0);
#endif
}

ocrSchedulerObject_t* domainSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    ASSERT(0); //Dynamic creation of domain object is not supported
    return NULL;
}

u8 domainSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(0); //Dynamic domain object is not supported
    return OCR_ENOTSUP;
}

u8 domainSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 domainSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 domainSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 domainSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ocrSchedulerObjectDomain_t *domainSchedObj = (ocrSchedulerObjectDomain_t*)self;
    ocrPolicyDomain_t *pd = fact->pd;
    u64 count = 0;
    if (properties & SCHEDULER_OBJECT_COUNT_EDT) {
        ocrSchedulerObjectFactory_t *wstFactory = pd->schedulerObjectFactories[domainSchedObj->wst->fctId];
        count += wstFactory->fcts.count(wstFactory, domainSchedObj->wst, properties);
    }
    return count;
}

ocrSchedulerObject_t* domainGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ocrSchedulerObjectDomain_t *domainSchedObj = (ocrSchedulerObjectDomain_t*)self;
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t *schedObj = NULL;
    if (properties & SCHEDULER_OBJECT_MAPPING_WST) {
        ocrSchedulerObjectFactory_t *wstFactory = pd->schedulerObjectFactories[domainSchedObj->wst->fctId];
        schedObj = wstFactory->fcts.getSchedulerObjectForLocation(wstFactory, domainSchedObj->wst, loc, mapping, properties);
    }
    if (properties & SCHEDULER_OBJECT_CREATE_IF_ABSENT)
        ASSERT(schedObj);
    return schedObj;
}

u8 domainSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* DOMAIN SCHEDULER_OBJECT ROOT FUNCTIONS             */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectDomain(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

ocrSchedulerObject_t* newSchedulerObjectRootDomain(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectDomain_t), PERSISTENT_CHUNK);
    schedObj->guid.guid = NULL_GUID; //Root objects are always static
    schedObj->guid.metaDataPtr = NULL;
    schedObj->kind = factory->kind;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;

    ocrSchedulerObjectDomain_t* domainSchedObj = (ocrSchedulerObjectDomain_t*)schedObj;
    domainSchedObj->dbTable = NULL;
    domainSchedObj->wst = NULL;

    return schedObj;
}

u8 domainSchedulerObjectSwitchRunlevel(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                    phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT((properties & RL_REQUEST) && !(properties & RL_RESPONSE)
           && !(properties & RL_RELEASE));
    ASSERT(!(properties & RL_FROM_MSG));

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if((properties & RL_BRING_UP) && RL_IS_FIRST_PHASE_UP(PD, RL_PD_OK, phase)) {
            u32 i;
            // The scheduler calls this before switching itself. Do we want
            // to invert this?
            for(i = 0; i < PD->schedulerObjectFactoryCount; ++i) {
                PD->schedulerObjectFactories[i]->pd = PD;
            }
        }
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        // Memory is up
        if(properties & RL_BRING_UP) {
            if(RL_IS_FIRST_PHASE_UP(PD, RL_GUID_OK, phase)) {
                domainSchedulerObjectInit(self, PD);
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
                domainSchedulerObjectFinish(self, PD);
            }
        }
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        ASSERT(0);
    }
    return toReturn;
    /* BUG #583: There was this code on STOP, not sure if we still need it
       ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)self;
       ocrSchedulerObjectFactory_t *fact = self->scheduler->pd->schedulerObjectFactories[schedObj->fctId];
       ASSERT(domainSchedulerObjectCount(fact, schedObj, 0) == 0);
    */
}

void domainSchedulerObjectDestruct(ocrSchedulerObject_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

ocrSchedulerObjectActionSet_t* domainSchedulerObjectNewActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, u32 count) {
    ASSERT(0);
    return NULL;
}

void domainSchedulerObjectDestroyActionSet(ocrSchedulerObject_t *self, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t *actionSet) {
    ASSERT(0);
    return;
}

/******************************************************/
/* OCR-DOMAIN SCHEDULER_OBJECT FACTORY FUNCTIONS      */
/******************************************************/

void destructSchedulerObjectRootFactoryDomain(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDomain(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ocrSchedulerObjectFactory_t* schedObjFact = NULL;
    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectRootFactoryDomain_t), PERSISTENT_CHUNK);
    } else {
        schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryDomain_t), PERSISTENT_CHUNK);
    }

    schedObjFact->factoryId = schedulerObjectDomain_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_DOMAIN;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectDomain;
    schedObjFact->destruct = &destructSchedulerObjectRootFactoryDomain;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), domainSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), domainSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), domainSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), domainSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), domainSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), domainSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), domainSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), domainGetSchedulerObjectForLocation);

    if (paramFact->kind == OCR_SCHEDULER_OBJECT_ROOT) {
        schedObjFact->kind |= OCR_SCHEDULER_OBJECT_ROOT;
        schedObjFact->instantiate = &newSchedulerObjectRootDomain;
        ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
        rootFactory->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), domainSchedulerObjectSwitchRunlevel);
        rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObject_t*), domainSchedulerObjectDestruct);

        rootFactory->fcts.newActionSet = FUNC_ADDR(ocrSchedulerObjectActionSet_t* (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, u32), domainSchedulerObjectNewActionSet);
        rootFactory->fcts.destroyActionSet = FUNC_ADDR(void (*)(ocrSchedulerObject_t*, ocrPolicyDomain_t *PD, ocrSchedulerObjectActionSet_t*), domainSchedulerObjectDestroyActionSet);
    }

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_DOMAIN */
