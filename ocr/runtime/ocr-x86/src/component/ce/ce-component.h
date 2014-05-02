/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_COMPONENT_H__
#define __CE_COMPONENT_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"


/****************************************************/
/* OCR CE COMPONENT TYPES                           */
/****************************************************/
#define OCR_COMP_PROP_TYPE_CE_STATE  0x10
#define OCR_COMP_PROP_TYPE_CE_WORK   0x20

/****************************************************/
/* OCR CE STATE COMPONENT                           */
/****************************************************/
#ifdef ENABLE_COMPONENT_CE_STATE

/*! \brief CE component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    deque_t * deque;
} ocrComponentCeState_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 dequeInitSize;
} ocrComponentFactoryCeState_t;

typedef struct _paramListComponentFactCeState_t {
    paramListComponentFact_t base;
    u32 dequeInitSize;
} paramListComponentFactCeState_t;

ocrComponentFactory_t * newOcrComponentFactoryCeState(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_CE_STATE */

#endif /* __CE_COMPONENT_H__ */
