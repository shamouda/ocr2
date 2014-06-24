/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HC_1_0_COMPONENT_H__
#define __HC_1_0_COMPONENT_H__

#include "ocr-config.h"

#ifdef ENABLE_COMPONENT_HC_1_0
#include "ocr-component.h"
#include "utils/deque.h"

typedef enum {
    HC_COMP_PROP_NONE = 0,
    HC_COMP_PROP_POP,
    HC_COMP_PROP_STEAL
} ocrCompPropHc_t;

/****************************************************/
/* OCR HC_1_0 COMPONENT                             */
/****************************************************/

/*! \brief HC_1_0 component implementation for a shared memory workstealing runtime
 */
typedef struct _ocrComponentHc_t {
    ocrComponent_t base;
    deque_t * deque;
} ocrComponentHc_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
} ocrComponentFactoryHc_t;

typedef struct _paramListComponentFactHc_t {
    paramListComponentFact_t base;
} paramListComponentFactHc_t;

ocrComponentFactory_t * newOcrComponentFactoryHc(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_HC_1_0 */

#endif /* __HC_1_0_COMPONENT_H__ */
