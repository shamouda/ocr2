/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "component/component-all.h"

const char * component_types[] = {
    "HC",
    "CE",
    "XE",
    NULL
};

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_COMPONENT_HC
    case componentHc_id:
        return newOcrComponentFactoryHc(perType);
#endif
#ifdef ENABLE_COMPONENT_CE
    case componentCe_id:
        return newOcrComponentFactoryCe(perType);
#endif
#ifdef ENABLE_COMPONENT_XE
    case componentXe_id:
        return newOcrComponentFactoryXe(perType);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

