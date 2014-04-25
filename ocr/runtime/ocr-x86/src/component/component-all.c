/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "component/component-all.h"
#include "debug.h"

const char * component_types[] = {
    "HC_STATE",
    "HC_WORK",
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
    default:
        ASSERT(0);
    }
    return NULL;
}

