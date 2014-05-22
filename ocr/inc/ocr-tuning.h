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

/**
 * @brief Placeholder for a hint in OCR
 *
 * @todo Define this. Not thought out.
 */
typedef u64 ocrHint_t;

/* priority level */
#define OCR_HINT_PRIORITY_MASK 0xFF
/* slot id of the input that provides max data access */
#define OCR_HINT_ACCESS0_SLOT_MASK 0xFF00
#define OCR_HINT_ACCESS0_SLOT_SHIFT 8
/* Percentage of the accesses for the input that provides max data access */
#define OCR_HINT_ACCESS0_WEIGHT_MASK 0xFF0000
#define OCR_HINT_ACCESS0_WEIGHT_SHIFT 16
/* slot id of the input that provides second highest data access */
#define OCR_HINT_ACCESS1_SLOT_MASK 0xFF000000
#define OCR_HINT_ACCESS1_SLOT_SHIFT 24
/* Percentage of the accesses for the input that provides second highest data access */
#define OCR_HINT_ACCESS1_WEIGHT_MASK 0xFF00000000
#define OCR_HINT_ACCESS1_WEIGHT_SHIFT 32

#define ocrHintCreate(guid)

#define ocrHintSetPriority(hint, pri) {hint = (hint & (~OCR_HINT_PRIORITY_MASK)) | (pri & 0xFF);}
#define ocrHintGetPriority(hint) (hint & OCR_HINT_PRIORITY_MASK)

#define ocrHintSetAccess0Slot(hint, slot) {hint = (hint & (~OCR_HINT_ACCESS0_SLOT_MASK)) | ((slot & 0xFF) << OCR_HINT_ACCESS0_SLOT_SHIFT);}
#define ocrHintGetAccess0Slot(hint) ((hint & OCR_HINT_ACCESS0_SLOT_MASK) >> OCR_HINT_ACCESS0_SLOT_SHIFT)

#define ocrHintSetAccess0Weight(hint, weight) {hint = (hint & (~OCR_HINT_ACCESS0_WEIGHT_MASK)) | ((weight & 0xFF) << OCR_HINT_ACCESS0_WEIGHT_SHIFT);}
#define ocrHintGetAccess0Weight(hint) ((hint & OCR_HINT_ACCESS0_WEIGHT_MASK) >> OCR_HINT_ACCESS0_WEIGHT_SHIFT)

#define ocrHintSetAccess1Slot(hint, slot) {hint = (hint & (~OCR_HINT_ACCESS1_SLOT_MASK)) | ((slot & 0xFF) << OCR_HINT_ACCESS1_SLOT_SHIFT);}
#define ocrHintGetAccess1Slot(hint) ((hint & OCR_HINT_ACCESS1_SLOT_MASK) >> OCR_HINT_ACCESS1_SLOT_SHIFT)

#define ocrHintSetAccess1Weight(hint, weight) {hint = (hint & (~OCR_HINT_ACCESS1_WEIGHT_MASK)) | ((weight & 0xFF) << OCR_HINT_ACCESS1_WEIGHT_SHIFT);}
#define ocrHintGetAccess1Weight(hint) ((hint & OCR_HINT_ACCESS1_WEIGHT_MASK) >> OCR_HINT_ACCESS1_WEIGHT_SHIFT)

#ifdef __cplusplus
}
#endif
/** TODO **/
#endif /* __OCR_TUNING_H__ */
