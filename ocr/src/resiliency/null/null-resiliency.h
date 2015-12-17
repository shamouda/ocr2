/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __NULL_RESILIENCY_H__
#define __NULL_RESILIENCY_H__

#include "ocr-config.h"
#ifdef ENABLE_RESILIENCY_NULL

#include "ocr-resiliency.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* NULL RESILIENCY                                  */
/****************************************************/

// Cached information about context
typedef struct _ocrResiliencyContextNull_t {
    ocrResiliencyContext_t base;
} ocrResiliencyContextNull_t;

typedef struct _ocrResiliencyNull_t {
    ocrResiliency_t base;
} ocrResiliencyNull_t;

/****************************************************/
/* NULL RESILIENCY FACTORY                          */
/****************************************************/

typedef struct _paramListResiliencyNull_t {
    paramListResiliency_t base;
} paramListResiliencyNull_t;

typedef struct _ocrResiliencyFactoryNull_t {
    ocrResiliencyFactory_t base;
} ocrResiliencyFactoryNull_t;

ocrResiliencyFactory_t * newOcrResiliencyFactoryNull(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_RESILIENCY_NULL */
#endif /* __NULL_RESILIENCY_H__ */

