/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __HC_1_0_COST_MAPPER_H__
#define __HC_1_0_COST_MAPPER_H__

#include "ocr-config.h"

#ifdef ENABLE_COST_MAPPER_HC_1_0

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "utils/ocr-utils.h"
#include "ocr-cost-mapper.h"
#include "ocr-component.h"
#include "component/hc/hc-component.h"

typedef struct {
    ocrCostMapperFactory_t base;
} ocrCostMapperFactoryHc_t;

typedef struct {
    ocrCostMapper_t base;
    ocrComponentFactory_t *componentFactory;
    ocrComponent_t **workerComponents;                    /* Per worker component */
    u32 numWorkers;                                       /* Number of workers managed by this cost mapper */
} ocrCostMapperHc_t;

typedef struct _paramListCostMapperHcInst_t {
    paramListCostMapperInst_t base;
} paramListCostMapperHcInst_t;

ocrCostMapperFactory_t * newOcrCostMapperFactoryHc(ocrParamList_t *perType);

#endif /* ENABLE_COST_MAPPER_HC_1_0 */
#endif /* __HC_1_0_COST_MAPPER_H__ */
