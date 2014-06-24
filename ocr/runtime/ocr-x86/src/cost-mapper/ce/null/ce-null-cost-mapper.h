/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_NULL_COST_MAPPER_H__
#define __CE_NULL_COST_MAPPER_H__

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_CE_NULL
#include "ocr-cost-mapper.h"

typedef struct {
    ocrCostMapperFactory_t base;
} ocrCostMapperFactoryCe_t;

typedef struct {
    ocrCostMapper_t base;
} ocrCostMapperCe_t;

typedef struct _paramListCostMapperCeInst_t {
    paramListCostMapperInst_t base;
} paramListCostMapperCeInst_t;

ocrCostMapperFactory_t * newOcrCostMapperFactoryCe(ocrParamList_t *perType);

#endif /* ENABLE_COST_MAPPER_CE_NULL */
#endif /* __CE_NULL_COST_MAPPER_H__ */
