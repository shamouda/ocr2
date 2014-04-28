/**
 * @brief Simple communication API
 *
 * Restrictions:
 *   - cannot poll on a specific handle
 *   - can only wait on the handle of the last sendMessage
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#ifndef __COMM_API_HC_H__
#define __COMM_API_HC_H__

#include "ocr-config.h"
#ifdef ENABLE_COMM_API_HC

#include "ocr-comm-api.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "utils/hashtable.h"

typedef struct {
    ocrCommApiFactory_t base;
} ocrCommApiFactoryHc_t;

typedef struct {
    ocrCommApi_t base;
    // Maps message 'ids' to handles
    hashtable_t * handleMap;
} ocrCommApiHc_t;

typedef struct {
    paramListCommApiInst_t base;
} paramListCommApiHc_t;

extern ocrCommApiFactory_t* newCommApiFactoryHc(ocrParamList_t *perType);

#endif /* ENABLE_COMM_API_HC */
#endif /* __COMM_API_HC_H__ */
