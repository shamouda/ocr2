/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "guid/guid-all.h"
#include "debug.h"

const char * guid_types[] = {
#ifdef ENABLE_GUID_PTR
    "PTR",
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
    "COUNTED_MAP",
#endif
#ifdef ENABLE_GUID_LABELED
    "LABELED",
#endif
    NULL
};

ocrGuidProviderFactory_t *newGuidProviderFactory(guidType_t type, ocrParamList_t *typeArg) {
    switch(type) {
#ifdef ENABLE_GUID_PTR
    case guidPtr_id:
        return newGuidProviderFactoryPtr(typeArg, (u32)type);
#endif
#ifdef ENABLE_GUID_COUNTED_MAP
    case guidCountedMap_id:
        return newGuidProviderFactoryCountedMap(typeArg, (u32)type);
#endif
#ifdef ENABLE_GUID_LABELED
    case guidLabeled_id:
        return newGuidProviderFactoryLabeled(typeArg, (u32)type);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}
