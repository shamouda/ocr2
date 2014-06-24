/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HC_NULL_COMPONENT_H__
#define __HC_NULL_COMPONENT_H__

#include "ocr-config.h"

#ifdef ENABLE_COMPONENT_HC_NULL
#include "ocr-component.h"

/****************************************************/
/* OCR HC_NULL COMPONENT                             */
/****************************************************/

/*! \brief HC_NULL component implementation for a shared memory workstealing runtime
 */
typedef struct _ocrComponentHc_t {
    ocrComponent_t base;
} ocrComponentHc_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
} ocrComponentFactoryHc_t;

typedef struct _paramListComponentFactHc_t {
    paramListComponentFact_t base;
} paramListComponentFactHc_t;

ocrComponentFactory_t * newOcrComponentFactoryHc(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_HC_NULL */

#endif /* __HC_NULL_COMPONENT_H__ */
