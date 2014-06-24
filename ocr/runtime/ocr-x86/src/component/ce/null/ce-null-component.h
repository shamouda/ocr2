/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_NULL_COMPONENT_H__
#define __CE_NULL_COMPONENT_H__

#include "ocr-config.h"

#ifdef ENABLE_COMPONENT_CE_NULL
#include "ocr-component.h"

/****************************************************/
/* OCR CE_NULL COMPONENT                             */
/****************************************************/

/*! \brief CE_NULL component implementation for a shared memory workstealing runtime
 */
typedef struct _ocrComponentCe_t {
    ocrComponent_t base;
} ocrComponentCe_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
} ocrComponentFactoryCe_t;

typedef struct _paramListComponentFactCe_t {
    paramListComponentFact_t base;
} paramListComponentFactCe_t;

ocrComponentFactory_t * newOcrComponentFactoryCe(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_CE_NULL */

#endif /* __CE_NULL_COMPONENT_H__ */
