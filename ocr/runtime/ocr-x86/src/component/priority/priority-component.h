/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __CE_PRIORITY_COMPONENT_H__
#define __CE_PRIORITY_COMPONENT_H__

#include "ocr-config.h"
#ifdef ENABLE_COMPONENT_PRIORITY

#include "utils/ocr-utils.h"
#include "utils/deque.h"
#include "ocr-component.h"

/****************************************************/
/* OCR PRIORITY COMPONENT                           */
/****************************************************/

typedef enum {
    AFFINITY_COMPONENT_STATE_NEW,
    AFFINITY_COMPONENT_STATE_STAGED,
    AFFINITY_COMPONENT_STATE_ACTIVE,
} ocrComponentAffinityState_t;
/*! \brief CE component implementation for a shared memory workstealing runtime
 */
typedef struct {
    ocrComponent_t base;
    deque_t **workpiles;   //deque on each priority level
    u32 numLevels;         //number of priority levels
    ocrGuid_t affinity;    //affinity id of this group
    u32 startLevel;        //lowest priority level that is active
    ocrComponentAffinityState_t state;
} ocrComponentPriority_t;

typedef struct {
    ocrComponentFactory_t baseFactory;
    u32 maxLevels;
} ocrComponentFactoryPriority_t;

typedef struct _paramListComponentFactPriority_t {
    paramListComponentFact_t base;
    u32 maxLevels;
} paramListComponentFactPriority_t;

ocrComponentFactory_t * newOcrComponentFactoryPriority(ocrParamList_t* perType);

#endif /* ENABLE_COMPONENT_PRIORITY */

#endif /* __CE_PRIORITY_COMPONENT_H__ */
