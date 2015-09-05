/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __WORKPILE_ALL_H__
#define __WORKPILE_ALL_H__

#include "ocr-config.h"

#include "utils/ocr-utils.h"
#include "ocr-workpile.h"

typedef enum _workpileType_t {
#ifdef ENABLE_WORKPILE_HC
    workpileHc_id,
#endif
#ifdef ENABLE_WORKPILE_CE
    workpileCe_id,
#endif
#ifdef ENABLE_WORKPILE_XE
    workpileXe_id,
#endif
    workpileMax_id,
} workpileType_t;

#ifdef ENABLE_WORKPILE_HC
#include "workpile/hc/hc-workpile.h"
#endif

#ifdef ENABLE_WORKPILE_CE
#include "workpile/ce/ce-workpile.h"
#endif

#ifdef ENABLE_WORKPILE_XE
#include "workpile/xe/xe-workpile.h"
#endif

extern const char * workpile_types[];

ocrWorkpileFactory_t * newWorkpileFactory(workpileType_t type, ocrParamList_t *perType);

#endif /* __WORKPILE_ALL_H__ */
