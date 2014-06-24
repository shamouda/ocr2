/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __XE_NULL_COST_MAPPER_H__
#define __XE_NULL_COST_MAPPER_H__

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_XE_NULL
#include "ocr-cost-mapper.h"

typedef struct {
    ocrCostMapperFactory_t base;
} ocrCostMapperFactoryXe_t;

typedef struct {
    ocrCostMapper_t base;
} ocrCostMapperXe_t;

typedef struct _paramListCostMapperXeInst_t {
    paramListCostMapperInst_t base;
} paramListCostMapperXeInst_t;

ocrCostMapperFactory_t * newOcrCostMapperFactoryXe(ocrParamList_t *perType);

#endif /* ENABLE_COST_MAPPER_XE_NULL */
#endif /* __XE_NULL_COST_MAPPER_H__ */
