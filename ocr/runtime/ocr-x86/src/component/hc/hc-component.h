/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HC_COMPONENT_H__
#define __HC_COMPONENT_H__

#include "ocr-config.h"
#include "ocr-component.h"

#ifdef ENABLE_COMPONENT_HC_STATE

/*! \brief HC component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    ocrComponent_t **components;
    u32 numWorkers;
} ocrComponentHcState_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 maxWorkers;
} ocrComponentFactoryHcState_t;

typedef struct _paramListComponentFactHcState_t {
    paramListComponentFact_t base;
    u32 maxWorkers;
} paramListComponentFactHcState_t;

ocrComponentFactory_t * newComponentFactoryHcState(ocrParamList_t* perType);
#endif /* ENABLE_COMPONENT_HC_STATE */

#ifdef ENABLE_COMPONENT_HC_WORK

/*! \brief HC component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    deque_t * deque;
} ocrComponentHcWork_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 dequeInitSize;
} ocrComponentFactoryHcWork_t;

typedef struct _paramListComponentFactHcWork_t {
    paramListComponentFact_t base;
    u32 dequeInitSize;
} paramListComponentFactHcState_t;

ocrComponentFactory_t * newComponentFactoryHcWork(ocrParamList_t* perType);
#endif /* ENABLE_COMPONENT_HC_WORK */

#endif /* __HC_COMPONENT_H__ */
