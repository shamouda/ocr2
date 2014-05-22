/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __WORKDEQUE_COMPONENT_H__
#define __WORKDEQUE_COMPONENT_H__

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_WORKDEQUE

#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"

/****************************************************/
/* OCR WORKDEQUE COMPONENT                          */
/****************************************************/

/*! \brief A deque based workpile
 */
typedef struct {
    ocrComponent_t base;
    deque_t * deque;
} ocrComponentWorkdeque_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 dequeInitSize;
} ocrComponentFactoryWorkdeque_t;

typedef struct _paramListComponentFactWorkdeque_t {
    paramListComponentFact_t base;
    u32 dequeInitSize;
} paramListComponentFactWorkdeque_t;

ocrComponentFactory_t * newOcrComponentFactoryWorkdeque(ocrParamList_t* perType);
#endif /* ENABLE_COMPONENT_WORKDEQUE */

#endif /* __WORKDEQUE_COMPONENT_H__ */
