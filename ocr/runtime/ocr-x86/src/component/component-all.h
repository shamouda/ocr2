/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __COMPONENT_ALL_H__
#define __COMPONENT_ALL_H__

#include "ocr-config.h"
#include "ocr-component.h"
//States
#include "component/wst/wst-component.h"
#include "component/ce/ce-component.h"
#include "component/ce/ce-par-component.h"
//Inners
#include "component/workdeque/workdeque-component.h"
#include "component/priority/priority-component.h"

typedef enum _componentType_t {
    componentWst_id,
    componentCe_id,
    componentWorkdeque_id,
    componentAffinity_id,
    componentPriority_id,
    componentMax_id
} componentType_t;

extern const char * component_types[];

ocrComponentFactory_t * newComponentFactory(componentType_t type, ocrParamList_t *perType);

#endif /* __COMPONENT_ALL_H__ */
