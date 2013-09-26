/**
 * @brief Default statistics collection which collects very basic information
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifdef OCR_ENABLE_STATISTICS

#ifndef __ENERGY_STATISTICS_H__
#define __ENERGY_STATISTICS_H__

#include "ocr-statistics.h"
#include "ocr-types.h"
#include "ocr-utils.h"

typedef struct {
    ocrStatsFactory_t base;
} ocrStatsFactoryEnergy_t;

typedef struct {
    ocrStats_t base;
    ocrStatsFilter_t *aggregatingFilter;
} ocrStatsEnergy_t;

extern ocrStatsFactory_t* newStatsFactoryEnergy(ocrParamList_t *perType);
#endif /* __ENERGY_STATISTICS_H__ */

#endif /* OCR_ENABLE_STATISTICS */
