/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_SCHEDULER_OBJECT_DBNODE

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-runtime-types.h"
#include "ocr-sysboot.h"
#include "ocr-task.h"
#include "scheduler-object/dbnode/dbnode-scheduler-object.h"
#include "scheduler-object/scheduler-object-all.h"

/******************************************************/
/* OCR-DBNODE SCHEDULER_OBJECT FUNCTIONS                        */
/******************************************************/

ocrSchedulerObject_t* dbNodeSchedulerObjectCreate(ocrSchedulerObjectFactory_t *fact, ocrParamList_t *params) {
    paramListSchedulerObject_t *paramSchedObj = (paramListSchedulerObject_t*)params;
    paramListSchedulerObjectDbNode_t *paramsDb = (paramListSchedulerObjectDbNode_t*)params;
    ASSERT(SCHEDULER_OBJECT_KIND(paramSchedObj->kind) == OCR_SCHEDULER_OBJECT_DBNODE);
    ASSERT(!paramSchedObj->guidRequired);
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObject_t* schedObj = (ocrSchedulerObject_t*)pd->fcts.pdMalloc(pd, sizeof(ocrSchedulerObjectDbNode_t));
    schedObj->guid.guid = NULL_GUID;
    schedObj->guid.metaDataPtr = schedObj;
    schedObj->kind = paramSchedObj->kind;
    schedObj->fctId = fact->factoryId;
    schedObj->loc = pd->myLocation;
    schedObj->mapping = OCR_SCHEDULER_OBJECT_MAPPING_PINNED;

    ocrSchedulerObjectDbNode_t* dbNodeSchedObj = (ocrSchedulerObjectDbNode_t*)schedObj;
#ifdef ENABLE_SCHEDULER_OBJECT_LIST
    paramListSchedulerObjectList_t paramList;
    paramList.base.kind = OCR_SCHEDULER_OBJECT_LIST | OCR_SCHEDULER_OBJECT_AGGREGATE;
    paramList.base.guidRequired = 0;

    //Create the phase list
    paramList.type = OCR_LIST_TYPE_SINGLE;
    paramList.elSize = sizeof(listDataPhase_t);
    paramList.arrayChunkSize = 8;
    ocrSchedulerObjectFactory_t *listFactory = pd->schedulerObjectFactories[schedulerObjectList_id];
    dbNodeSchedObj->phaseList = listFactory->fcts.create(listFactory, (ocrParamList_t*)(&paramList));

    //Create initial phase node
    ocrSchedulerObject_t phaseObj;
    phaseObj.guid.guid = NULL_GUID;
    phaseObj.guid.metaDataPtr = NULL;
    phaseObj.kind = OCR_SCHEDULER_OBJECT_VOID_PTR;
    listFactory->fcts.insert(listFactory, dbNodeSchedObj->phaseList, &phaseObj, SCHEDULER_OBJECT_INSERT_LIST_FRONT);
    ASSERT(phaseObj.guid.metaDataPtr);
    listDataPhase_t *phaseData = (listDataPhase_t*)phaseObj.guid.metaDataPtr;

    //Initialize the phase node
    phaseData->phase = paramsDb->initialPhase;
    phaseData->loc = pd->myLocation;
    paramList.type = OCR_LIST_TYPE_SINGLE;
    paramList.elSize = sizeof(listDataEdt_t);
    phaseData->waitList = listFactory->fcts.create(listFactory, (ocrParamList_t*)(&paramList));

    //Create the active EDT list
    paramList.type = OCR_LIST_TYPE_SINGLE;
    paramList.elSize = sizeof(listDataEdt_t);
    paramList.arrayChunkSize = 8;
    dbNodeSchedObj->activeList = listFactory->fcts.create(listFactory, (ocrParamList_t*)(&paramList));
#else
    ASSERT(0);
#endif

    dbNodeSchedObj->executionClock = 0;
    dbNodeSchedObj->scheduleClock = 0;
    dbNodeSchedObj->currentPhase = paramsDb->initialPhase;
    dbNodeSchedObj->currentLoc = pd->myLocation;
    dbNodeSchedObj->dbSize = paramsDb->dbSize;
    dbNodeSchedObj->dataPtr = paramsDb->dataPtr;
    dbNodeSchedObj->lock = 0;
    return schedObj;
}

u8 dbNodeSchedulerObjectDestroy(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self) {
    ocrPolicyDomain_t *pd = fact->pd;
    ocrSchedulerObjectDbNode_t* dbNodeSchedObj = (ocrSchedulerObjectDbNode_t*)self;
    ocrSchedulerObjectFactory_t *listFactory = pd->schedulerObjectFactories[dbNodeSchedObj->phaseList->fctId];
    listFactory->fcts.destroy(listFactory, dbNodeSchedObj->phaseList);
    listFactory->fcts.destroy(listFactory, dbNodeSchedObj->activeList);
    pd->fcts.pdFree(pd, self);
    return 0;
}

u8 dbNodeSchedulerObjectInsert(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 dbNodeSchedulerObjectRemove(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectKind kind, u32 count, ocrSchedulerObject_t *dst, ocrSchedulerObject_t *element, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u8 dbNodeSchedulerObjectIterate(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrSchedulerObjectIterator_t *iterator, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

u64 dbNodeSchedulerObjectCount(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, u32 properties) {
    ASSERT(0);
    return OCR_ENOTSUP;
}

ocrSchedulerObject_t* dbNodeGetSchedulerObjectForLocation(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping, u32 properties) {
    ASSERT(0);
    return NULL;
}

u8 dbNodeSetLocationForSchedulerObject(ocrSchedulerObjectFactory_t *fact, ocrSchedulerObject_t *self, ocrLocation_t loc, ocrSchedulerObjectMappingKind mapping) {
    self->loc = loc;
    self->mapping = mapping;
    return 0;
}

/******************************************************/
/* OCR-DBNODE SCHEDULER_OBJECT FACTORY FUNCTIONS                */
/******************************************************/

ocrSchedulerObject_t* newSchedulerObjectDbNode(ocrSchedulerObjectFactory_t *factory, ocrParamList_t *perInstance) {
    return NULL;
}

void destructSchedulerObjectFactoryDbNode(ocrSchedulerObjectFactory_t * factory) {
    runtimeChunkFree((u64)factory, PERSISTENT_CHUNK);
}

ocrSchedulerObjectFactory_t * newOcrSchedulerObjectFactoryDbNode(ocrParamList_t *perType, u32 factoryId) {
    paramListSchedulerObjectFact_t *paramFact = (paramListSchedulerObjectFact_t*)perType;
    ASSERT(paramFact->kind == OCR_SCHEDULER_OBJECT_AGGREGATE);
    ocrSchedulerObjectFactory_t* schedObjFact = (ocrSchedulerObjectFactory_t*) runtimeChunkAlloc(
                                      sizeof(ocrSchedulerObjectFactoryDbNode_t), PERSISTENT_CHUNK);

    schedObjFact->factoryId = schedulerObjectDbNode_id;
    schedObjFact->kind = OCR_SCHEDULER_OBJECT_DBNODE;
    schedObjFact->pd = NULL;

    schedObjFact->instantiate = &newSchedulerObjectDbNode;
    schedObjFact->destruct = &destructSchedulerObjectFactoryDbNode;

    schedObjFact->fcts.create = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrParamList_t*), dbNodeSchedulerObjectCreate);
    schedObjFact->fcts.destroy = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*), dbNodeSchedulerObjectDestroy);
    schedObjFact->fcts.insert = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), dbNodeSchedulerObjectInsert);
    schedObjFact->fcts.remove = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectKind, u32, ocrSchedulerObject_t*, ocrSchedulerObject_t*, u32), dbNodeSchedulerObjectRemove);
    schedObjFact->fcts.iterate = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrSchedulerObjectIterator_t*, u32), dbNodeSchedulerObjectIterate);
    schedObjFact->fcts.count = FUNC_ADDR(u64 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, u32), dbNodeSchedulerObjectCount);
    schedObjFact->fcts.setLocationForSchedulerObject = FUNC_ADDR(u8 (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind), dbNodeSetLocationForSchedulerObject);
    schedObjFact->fcts.getSchedulerObjectForLocation = FUNC_ADDR(ocrSchedulerObject_t* (*)(ocrSchedulerObjectFactory_t*, ocrSchedulerObject_t*, ocrLocation_t, ocrSchedulerObjectMappingKind, u32), dbNodeGetSchedulerObjectForLocation);

    return schedObjFact;
}

#endif /* ENABLE_SCHEDULER_OBJECT_DBNODE */
