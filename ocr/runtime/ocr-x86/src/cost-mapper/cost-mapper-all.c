/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "cost-mapper/cost-mapper-all.h"

const char * costMapper_types[] = {
    "HC",
    "CE",
    "XE",
    NULL
};

ocrCostMapperFactory_t * newCostMapperFactory(costMapperType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_COST_MAPPER_HC
    case costMapperHc_id:
        return newOcrCostMapperFactoryHc(perType);
#endif
#ifdef ENABLE_COST_MAPPER_CE
    case costMapperCe_id:
        return newOcrCostMapperFactoryCe(perType);
#endif
#ifdef ENABLE_COST_MAPPER_XE
    case costMapperXe_id:
        return newOcrCostMapperFactoryXe(perType);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

