/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COST_MAPPER_ALL_H__
#define __COST_MAPPER_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"

#include "ocr-cost-mapper.h"
#include "cost-mapper/hc/hc-cost-mapper.h"
#include "cost-mapper/ce/ce-cost-mapper.h"

typedef enum _costMapperType_t {
    costMapperHc_id,
    costMapperCe_id,
    costMapperMax_id
} costMapperType_t;

extern const char * costMapper_types[];

ocrCostMapperFactory_t* newCostMapperFactory(costMapperType_t type, ocrParamList_t *perType);

#endif /* __COST_MAPPER_ALL_H__ */
