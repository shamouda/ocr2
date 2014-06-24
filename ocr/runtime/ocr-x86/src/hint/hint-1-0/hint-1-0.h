/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HINT_1_0_H__
#define __HINT_1_0_H__

#include "ocr-config.h"
#ifdef ENABLE_HINT_1_0

#include "ocr-runtime-types.h"
#include "utils/ocr-utils.h"
#include "ocr-hint.h"

/****************************************************/
/* OCR RUNTIME HINTS PARAMETER LIST                 */
/****************************************************/

typedef struct _paramListHint_1_0_t {
    ocrParamListHint_t base;
    u32 priority;
    ocrFatGuid_t component;
    ocrLocation_t mapping;
} ocrParamListHint_1_0_t;

/****************************************************/
/* OCR HINT FACTORY                                 */
/****************************************************/

typedef struct _ocrHintFactory_1_0_t {
    ocrHintFactory_t baseFactory;
} ocrHintFactory_1_0_t;

ocrHintFactory_t * newHintFactory_1_0(ocrParamList_t* perType, u32 factoryId);

#endif /* ENABLE_HINT_1_0 */
#endif /* __HINT_1_0_H__ */
