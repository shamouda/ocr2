/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMPONENT_ALL_H__
#define __COMPONENT_ALL_H__

#include "ocr-config.h"
#include "utils/ocr-utils.h"

#include "ocr-component.h"
#include "component/hc/hc-component.h"

typedef enum _componentType_t {
    componentHcState_id,
    componentHcWork_id,
    componentMax_id
} componentType_t;

extern const char * component_types[];

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType);

#endif /* __COMPONENT_ALL_H__ */
