/**
 * @brief Tuning language API for OCR
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_TUNING_H__
#define __OCR_TUNING_H__
#ifdef __cplusplus
extern "C" {
#endif
#include "ocr-types.h"

u8 ocrHintCreate(ocrGuid_t *guid, ocrHintTypes_t hintType);

u8 ocrHintSetPriority(ocrGuid_t guid, u32 priority);

u8 ocrHintSetDependenceWeight(ocrGuid_t guid, u32 depNum, u32 depWeight);

u8 ocrSetHint(ocrGuid_t guid, ocrGuid_t hint);

u8 ocrGetHint(ocrGuid_t guid, ocrGuid_t *hint);

#ifdef __cplusplus
}
#endif
/** TODO **/
#endif /* __OCR_TUNING_H__ */
