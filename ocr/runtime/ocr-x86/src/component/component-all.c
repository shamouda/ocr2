/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "debug.h"
#include "component/component-all.h"

const char * component_types[] = {
    "HC_STATE",
    "HC_WORK",
    "CE_STATE",
    NULL
};

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType) {
    switch(type) {
#ifdef ENABLE_COMPONENT_HC_STATE
    case componentHcState_id:
        return newOcrComponentFactoryHcState(perType);
#endif
#ifdef ENABLE_COMPONENT_HC_WORK
    case componentHcWork_id:
        return newOcrComponentFactoryHcWork(perType);
#endif
#ifdef ENABLE_COMPONENT_CE_STATE
    case componentCeState_id:
        return newOcrComponentFactoryCeState(perType);
#endif
    default:
        ASSERT(0);
    }
    return NULL;
}

