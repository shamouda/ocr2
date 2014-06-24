/**
 * @brief Tuning language API for OCR
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_AFFINITY_H__
#define __OCR_AFFINITY_H__

#ifdef __cplusplus
extern "C" {
#endif
// places include RAM, SP, or NUMA if available

#include "ocr-types.h"

//
// User-level API (experimental - may be deprecated anytime)
//

typedef enum {
    AFFINITY_PD,
    AFFINITY_PD_MASTER
} ocrAffinityKind;

/**
 * Returns a count of affinity guids of a particular kind.
 *
 * @param[in] kind             The affinity kind to query for
 * @param[out] count           Count of affinity guid of that kind
 */
u8 ocrAffinityCount(ocrAffinityKind kind, u64 * count);

/**
 * Get affinity guids of a particular kind. The 'affinities' array must have
 * been previously allocated.
 *
 * @param[in] kind             The affinity kind to query for
 * @param[in,out] count        In: Requested number of element, Out: Actual element returned
 * @param[out] affinities      Affinity guid array, call-site allocated.
 */
u8 ocrAffinityGet(ocrAffinityKind kind, u64 * count, ocrGuid_t * affinities);

#ifdef __cplusplus
}
#endif

#endif /* __OCR_AFFINITY_H__ */
