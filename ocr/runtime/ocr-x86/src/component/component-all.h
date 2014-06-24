/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMPONENT_ALL_H__
#define __COMPONENT_ALL_H__

#include "ocr-config.h"
#include "ocr-hint.h"
#include "ocr-component.h"
#include "component/hc/hc-component.h"
#include "component/ce/ce-component.h"
#include "component/xe/xe-component.h"

typedef enum _componentType_t {
    componentHc_id,
    componentCe_id,
    componentXe_id,
    componentMax_id
} componentType_t;

extern const char * component_types[];

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType);

#endif /* __COMPONENT_ALL_H__ */
