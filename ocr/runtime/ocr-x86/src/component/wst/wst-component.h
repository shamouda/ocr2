/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __WST_COMPONENT_H__
#define __WST_COMPONENT_H__

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_WST

#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"

/****************************************************/
/* OCR WST STATE COMPONENT                           */
/****************************************************/

/*! \brief WST component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    ocrComponent_t **components;
    u32 numWorkers;
} ocrComponentWst_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 maxWorkers;
} ocrComponentFactoryWst_t;

typedef struct _paramListComponentFactWst_t {
    paramListComponentFact_t base;
    u32 maxWorkers;
} paramListComponentFactWst_t;

ocrComponentFactory_t * newOcrComponentFactoryWst(ocrParamList_t* perType);
#endif /* ENABLE_COMPONENT_WST */

#endif /* __WST_COMPONENT_H__ */
