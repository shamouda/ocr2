/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_PAR_COMPONENT_H__
#define __CE_PAR_COMPONENT_H__

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_CE_PAR

#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"

/****************************************************/
/* OCR CE PAR COMPONENT                           */
/****************************************************/

/*! \brief CE component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    ocrComponent_t **affinities; //Affinity groups
    ocrComponent_t **workpiles;  //Active workpile on each worker
    u32 numWorkers;
    u32 numGroups;
} ocrComponentCePar_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 maxWorkers;
    u32 maxGroups;
} ocrComponentFactoryCePar_t;

typedef struct _paramListComponentFactCePar_t {
    paramListComponentFact_t base;
    u32 maxWorkers;
    u32 maxGroups;
} paramListComponentFactCePar_t;

ocrComponentFactory_t * newOcrComponentFactoryCe(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_CE_PAR */

#endif /* __CE_PAR_COMPONENT_H__ */
