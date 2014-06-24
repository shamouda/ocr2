/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __XE_NULL_COMPONENT_H__
#define __XE_NULL_COMPONENT_H__

#include "ocr-config.h"

#ifdef ENABLE_COMPONENT_XE_NULL
#include "ocr-component.h"

/****************************************************/
/* OCR XE_NULL COMPONENT                             */
/****************************************************/

/*! \brief XE_NULL component implementation for a shared memory workstealing runtime
 */
typedef struct _ocrComponentXe_t {
    ocrComponent_t base;
} ocrComponentXe_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
} ocrComponentFactoryXe_t;

typedef struct _paramListComponentFactXe_t {
    paramListComponentFact_t base;
} paramListComponentFactXe_t;

ocrComponentFactory_t * newOcrComponentFactoryXe(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_XE_NULL */

#endif /* __XE_NULL_COMPONENT_H__ */
