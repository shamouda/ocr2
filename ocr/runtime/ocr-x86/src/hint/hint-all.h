/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HINT_ALL_H__
#define __HINT_ALL_H__

#include "ocr-config.h"
#include "debug.h"
#include "utils/ocr-utils.h"
#include "ocr-hint.h"

typedef enum _hintType_t {
    hint_1_0_id,
    hintMax_id
} hintType_t;

extern const char * hint_types[];

ocrHintFactory_t *newHintFactory(hintType_t type, ocrParamList_t *typeArg);

#endif /* __HINT_ALL_H__ */
