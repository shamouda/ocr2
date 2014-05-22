/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "component/component-all.h"

const char * component_types[] = {
    "WST",
    "CE",
    "WORKDEQUE",
    "AFFINITY",
    "PRIORITY",
    NULL
};

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_COMPONENT_WST
    case componentWst_id:
        return newOcrComponentFactoryWst(perType);
#endif
#ifdef ENABLE_COMPONENT_CE
    case componentCe_id:
        return newOcrComponentFactoryCe(perType);
#endif
#ifdef ENABLE_COMPONENT_WORKDEQUE
    case componentWorkdeque_id:
        return newOcrComponentFactoryWorkdeque(perType);
#endif
#ifdef ENABLE_COMPONENT_AFFINITY
    case componentAffinity_id:
        return newOcrComponentFactoryAffinity(perType);
#endif
#ifdef ENABLE_COMPONENT_PRIORITY
    case componentPriority_id:
        return newOcrComponentFactoryPriority(perType);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

