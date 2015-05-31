/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_WST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/wst/wst-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-WST SCHEDULER_OBJECT FUNCTIONS                        */
/******************************************************/

ocrSchedulerObject_t* wstSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    u32 i;
    ocrSchedulerObject_t *el = NULL;
    paramListSchedulerObject_t *schedObjParams = (paramListSchedulerObject_t*)params;
    ASSERT(schedObjParams->kind != OCR_SCHEDULER_OBJECT_ROOT_WST);
    ocrPolicyDomain_t *pd = fact->pd;
    switch(schedObjParams->kind) {
    case OCR_SCHEDULER_OBJECT_EDT:
    case OCR_SCHEDULER_OBJECT_DB:
        {   //schedulerObject->guid is used to store singleton guid
            el = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObject_t));
            el->guid = NULL_GUID;
            el->kind = schedObjParams->kind;
            el->fctId = fact->factoryId;
            el->loc = INVALID_LOCATION;
            el->mapping = OCR_SCHEDULER_OBJECT_MAPPING_UNDEFINED;
        }
        break;
    default:
        {
            ocrSchedulerObjectFactory_t **factories = pd->schedulerObjectFactories;
            for (i = 0; (i < pd->schedulerObjectFactoryCount) && (el == NULL); i++) {
                ocrSchedulerObjectFactory_t *factory = factories[i];
                el = factory->fcts.create(factory, params);
            }
        }
        break;
    }
    return el;
}

u8 wstSchedulerObjectDestruct(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ASSERT(IS_SCHEDULER_OBJECT_SINGLETON_KIND(self->kind));
    ocrPolicyDomain_t *pd = fact->pd;
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 wstSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(IS_SCHEDULER_OBJECT_SINGLETON_KIND(self->kind)); //Using the schedulerObject root to invoke singleton function which do not have factories
    ASSERT(self->kind == element->kind);
    self->guid = element->guid;
    return 0;
}

u8 wstSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 wstSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    u64 count = 0;
    u32 i;
    ocrSchedulerObjectRootWst_t *wstSchedObj = (ocrSchedulerObjectRootWst_t*)self;
    ocrPolicyDomain_t *pd = fact->pd;
    for (i = 0; i < wstSchedObj->numDeques; i++) {
        ocrSchedulerObject_t *deque = wstSchedObj->deques[i];
        ocrSchedulerObjectFactory_t *dequeFactory = pd->schedulerObjectFactories[deque->fctId];
        count += dequeFactory->fcts.count(dequeFactory, deque, properties);
    }
    return count;
}

ocrSchedulerObject_t* wstGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    u32 i;
    ocrSchedulerObjectRootWst_t *wstSchedObj = (ocrSchedulerObjectRootWst_t*)self;
    for (i = 0; i < wstSchedObj->numDeques; i++) {
        if (wstSchedObj->deques[i]->loc == loc && wstSchedObj->deques[i]->mapping == mapping)
            return wstSchedObj->deques[i];
    }
    return NULL;
}

u8 wstSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* WST SCHEDULER_OBJECT ROOT FUNCTIONS                      */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectRootWst(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    ocrSchedulerObject_t *schedObj = (ocrSchedulerObject_t*)runtimeChunkAlloc(sizeof(ocrSchedulerObjectRootWst_t), PERSISTENT_CHUNK);
    schedObj->guid = NULL_GUID;
    schedObj->kind = OCR_SCHEDULER_OBJECT_ROOT_WST;
    schedObj->fctId = factory->factoryId;
    schedObj->loc = INVALID_LOCATION;

    ocrSchedulerObjectRoot_t *rootSchedObj = (ocrSchedulerObjectRoot_t*)schedObj;
    ocrSchedulerObjectRootFactory_t *rootFact = (ocrSchedulerObjectRootFactory_t*)factory;
    rootSchedObj->scheduler = NULL;
    rootSchedObj->fcts = rootFact->fcts;

    ocrSchedulerObjectRootWst_t* wstRootSchedObj = (ocrSchedulerObjectRootWst_t*)schedObj;
    wstRootSchedObj->numDeques = 0;
    wstRootSchedObj->deques = NULL;

    return schedObj;
}

u8 wstSchedulerObjectSwitchRunlevel(ocrSchedulerObjectRoot_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
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
            ASSERT(self->scheduler);
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
                u32 i;
                ocrScheduler_t *scheduler = self->scheduler;

                ocrSchedulerObjectRootWst_t *wstRootSchedObj = (ocrSchedulerObjectRootWst_t*)self;
                wstRootSchedObj->numDeques = scheduler->contextCount;
                wstRootSchedObj->deques = (ocrSchedulerObject_t**)PD->fcts.pdMalloc(
                    PD, wstRootSchedObj->numDeques * sizeof(ocrSchedulerObject_t*));
#ifdef ENABLE_SCHEDULER_OBJECT_DEQ
                //Instantiate the deque schedulerObjects
                paramListSchedulerObjectDeq_t params;
                params.base.kind = OCR_SCHEDULER_OBJECT_DEQUE;
                params.base.count = 1;
                params.type = WORK_STEALING_DEQUE;
                ocrSchedulerObjectFactory_t *dequeFactory = PD->schedulerObjectFactories[schedulerObjectDeq_id];
                for (i = 0; i < wstRootSchedObj->numDeques; i++) {
                    wstRootSchedObj->deques[i] = dequeFactory->fcts.create(dequeFactory, (ocrParamList_t*)(&params));
                }
#else
                ASSERT(0);
#endif
            }
        } else {
            // Tear down
            if(RL_IS_LAST_PHASE_DOWN(PD, RL_GUID_OK, phase)) {
                u32 i;
                ocrSchedulerObjectRootWst_t *wstRootSchedObj = (ocrSchedulerObjectRootWst_t*)self;
                for (i = 0; i < wstRootSchedObj->numDeques; i++) {
                    ocrSchedulerObject_t *deque = wstRootSchedObj->deques[i];
                    ocrSchedulerObjectFactory_t *dequeFactory = PD->schedulerObjectFactories[deque->fctId];
                    dequeFactory->fcts.destruct(dequeFactory, wstRootSchedObj->deques[i]);
                }
                PD->fcts.pdFree(PD, wstRootSchedObj->deques);
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
       ASSERT(wstSchedulerObjectCount(fact, schedObj, 0) == 0);
    */
}

void wstSchedulerObjectRootDestruct(ocrSchedulerObjectRoot_t *self) {
    runtimeChunkFree((u64)self, PERSISTENT_CHUNK);
}

/******************************************************/
/* OCR-WST SCHEDULER_OBJECT FACTORY FUNCTIONS         */
/******************************************************/

void destructSchedulerObjectRootFactoryWst(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

static ocrSchedulerObjectFactory_t* newOcrSchedulerObjectRootFactoryWst(ocrParamList_t *perType, u32 factoryId) {
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectRootFactoryWst_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectWst_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_ROOT_WST;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectRootWst;
    schedObjFact->destruct = &destructSchedulerObjectRootFactoryWst;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), wstSchedulerObjectCreate);
    schedObjFact->fcts.destruct = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), wstSchedulerObjectDestruct);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectRemove);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), wstSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), wstSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), wstGetSchedulerObjectForLocation);

    ocrSchedulerObjectRootFactory_t* rootFactory = (ocrSchedulerObjectRootFactory_t*)schedObjFact;
    rootFactory->fcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrSchedulerObjectRoot_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                        phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), wstSchedulerObjectSwitchRunlevel);
    rootFactory->fcts.destruct = FUNC_ADDR(void (*)(ocrSchedulerObjectRoot_t*), wstSchedulerObjectRootDestruct);

    return schedObjFact;
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryWst(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    switch(SCHEDULER_OBJECT_KIND_TYPE(paramFact->kind)) {
    case OCR_SCHEDULER_OBJECT_ROOT:
        return newOcrSchedulerObjectRootFactoryWst(perType, factoryId);
    default:
        ASSERT(0);
        break;
    }
    return NULL;
}

#endif /* ENABLE_SCHEDULER_OBJECT_WST */
