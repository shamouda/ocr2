/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HC_NULL_COST_MAPPER_H__
#define __HC_NULL_COST_MAPPER_H__

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_HC_NULL
#include "ocr-cost-mapper.h"

typedef struct {
    ocrCostMapperFactory_t base;
} ocrCostMapperFactoryHc_t;

typedef struct {
    ocrCostMapper_t base;
} ocrCostMapperHc_t;

typedef struct _paramListCostMapperHcInst_t {
    paramListCostMapperInst_t base;
} paramListCostMapperHcInst_t;

ocrCostMapperFactory_t * newOcrCostMapperFactoryHc(ocrParamList_t *perType);

#endif /* ENABLE_COST_MAPPER_HC_NULL */
#endif /* __HC_NULL_COST_MAPPER_H__ */
