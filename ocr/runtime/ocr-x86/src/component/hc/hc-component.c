/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_HC_STATE

ocrComponent_t* newComponentHcState(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    u32 i;

    ocrComponentFactoryHcState_t * derivedFactory = (ocrComponentFactoryHcState_t*)factory;
    u32 numWorkers = derivedFactory->maxWorkers;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentHcState_t * component = pd->fcts.pdMalloc(pd, sizeof(ocrComponentHcState_t) + (numWorkers * sizeof(ocrComponent_t*)));
    component->components = (ocrComponent_t **)((u64)component + sizeof(ocrComponentHcState_t));
    component->numWorkers = numWorkers;
    for (i = 0; i < numWorkers; i++)
        component->components[i] = NULL;
}

ocrComponentFactory_t* newOcrComponentFactoryHcState(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryHcState_t), (void *)1);

    base->instantiate = &newComponentHcState;
    base->destruct = &destructComponentFactoryHcState;

    base->fcts.create = NULL;
    base->fcts.insert = NULL;
    base->fcts.remove = NULL;
    base->fcts.move = NULL;
    base->fcts.split = NULL;
    base->fcts.merge = NULL;
    base->fcts.count = NULL;
/*
    base->fcts.create = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t*, ocrFatGuid_t, u32), hcStateComponentCreate);
    base->fcts.insert = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcStateComponentInsert);
    base->fcts.remove = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcStateComponentRemove);
    base->fcts.move = FUNC_ADDR(void (*)(ocrComponent_t*, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcStateComponentMove);
    base->fcts.split = FUNC_ADDR(void (*)(ocrComponent_t*, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), hcStateComponentSplit);
    base->fcts.merge = FUNC_ADDR(void (*)(ocrComponent_t*, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), hcStateComponentMerge);
    base->fcts.count = FUNC_ADDR(void (*)(ocrComponent_t*), hcStateComponentCount);
*/
    paramListComponentFactHcState_t * paramDerived = (paramListComponentFactHcState_t*)perType;
    ocrComponentFactoryHcState_t * derived = (ocrComponentFactoryHcState_t*)base;
    derived->maxWorkers = paramDerived->maxWorkers;
    return base;
}

#endif /* ENABLE_COMPONENT_HC_STATE */

#ifdef ENABLE_COMPONENT_HC_WORK

ocrComponent_t* newComponentHcWork(ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties) {
    u32 i;

    //ocrComponentFactoryHcWork_t * derivedFactory = (ocrComponentFactoryHcWork_t*)factory;
    //u32 dequeInitSize = derivedFactory->dequeInitSize;
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrComponentHcWork_t * component = pd->fcts.pdMalloc(pd, sizeof(ocrComponentHcWork_t));
    component->deque = newWorkStealingDeque(pd, (void *) NULL_GUID);
}

ocrComponentFactory_t* newOcrComponentFactoryHcWork(ocrParamList_t *perType) {
    ocrComponentFactory_t* base = (ocrComponentFactory_t*) runtimeChunkAlloc(
        sizeof(ocrComponentFactoryHcWork_t), (void *)1);

    base->instantiate = &newComponentHcWork;
    base->destruct = &destructComponentFactoryHcWork;

    base->fcts.create = NULL;
    base->fcts.insert = NULL;
    base->fcts.remove = NULL;
    base->fcts.move = NULL;
    base->fcts.split = NULL;
    base->fcts.merge = NULL;
    base->fcts.count = NULL;
/*
    base->fcts.create = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t*, ocrFatGuid_t, u32), hcWorkComponentCreate);
    base->fcts.insert = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcWorkComponentInsert);
    base->fcts.remove = FUNC_ADDR(void (*)(ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcWorkComponentRemove);
    base->fcts.move = FUNC_ADDR(void (*)(ocrComponent_t*, ocrComponent_t*, ocrFatGuid_t, ocrFatGuid_t, u32), hcWorkComponentMove);
    base->fcts.split = FUNC_ADDR(void (*)(ocrComponent_t*, u32, u32*, ocrFatGuid_t*, ocrFatGuid_t, u32), hcWorkComponentSplit);
    base->fcts.merge = FUNC_ADDR(void (*)(ocrComponent_t*, u32, ocrFatGuid_t*, ocrFatGuid_t, u32), hcWorkComponentMerge);
    base->fcts.count = FUNC_ADDR(void (*)(ocrComponent_t*), hcWorkComponentCount);
*/
    paramListComponentFactHcWork_t * paramDerived = (paramListComponentFactHcWork_t*)perType;
    ocrComponentFactoryHcWork_t * derived = (ocrComponentFactoryHcWork_t*)base;
    derived->dequeInitSize = paramDerived->dequeInitSize;
    return base;
}

#endif /* ENABLE_COMPONENT_HC_WORK */
