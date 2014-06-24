/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_1_0_COMPONENT_H__
#define __CE_1_0_COMPONENT_H__

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_1_0

#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"

#define COMPONENT_ALLOC_SIZE 16384
#define COMPONENT_ELEMENT_ALLOC_SIZE 1024

/****************************************************/
/* OCR CE_1_0 COMPONENT                             */
/****************************************************/
struct _ocrComponentCe_1_0_t;

typedef struct _ocrComponentEl_1_0_t {
    ocrGuid_t data;
    struct _ocrComponentEl_1_0_t *next;
    //struct _ocrComponentCe_1_0_t *parent;
} ocrComponentEl_1_0_t;

/*! \brief CE_1_0 component implementation for a shared memory workstealing runtime
 */
typedef struct _ocrComponentCe_1_0_t {
    ocrComponent_t base;
    ocrComponentEl_1_0_t *pElementHead;           //elements that are this component's children
    struct _ocrComponentCe_1_0_t *pComponentHead; //components that are this component's children
    struct _ocrComponentCe_1_0_t *next;           //next sibling component
    //struct _ocrComponentCe_1_0_t *parent;       //parent component
} ocrComponentCe_1_0_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    ocrFatGuid_t *componentBlock;
    u64 componentBlockIndex;
    ocrComponentCe_1_0_t *pFreeComponentHead;
    ocrComponentEl_1_0_t *elementBlock;
    u64 elementBlockIndex;
    ocrComponentEl_1_0_t *pFreeElementHead;
} ocrComponentFactoryCe_1_0_t;

typedef struct _paramListComponentFactCe_1_0_t {
    paramListComponentFact_t base;
} paramListComponentFactCe_1_0_t;

ocrComponentFactory_t * newOcrComponentFactoryCe(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_CE_1_0 */

#endif /* __CE_1_0_COMPONENT_H__ */
