/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __GUID_ALL_H__
#define __GUID_ALL_H__

#include "debug.h"
#include "ocr-config.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

typedef enum _guidType_t {
#ifdef ENABLE_GUID_PTR
    guidPtr_id,
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
    guidCountedMap_id,
#endif
#ifdef ENABLE_GUID_LABELED
    guidLabeled_id,
#endif
    guidMax_id
} guidType_t;

#ifdef ENABLE_GUID_PTR
#include "guid/ptr/ptr-guid.h"
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
#include "guid/counted/counted-map-guid.h"
#endif
#ifdef ENABLE_GUID_LABELED
#include "guid/labeled/labeled-guid.h"
#endif

extern const char * guid_types[];

ocrGuidProviderFactory_t *newGuidProviderFactory(guidType_t type, ocrParamList_t *typeArg);

#endif /* __GUID_ALL_H__ */
