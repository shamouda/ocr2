/**
 * @brief Tuning language API for OCR. This is an experimental feature
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_AFFINITY_H__
#define __OCR_AFFINITY_H__
#ifdef ENABLE_EXTENSION_AFFINITY

#ifdef __cplusplus
extern "C" {
#endif
// places include RAM, SP, or NUMA if available

#include "ocr-types.h"

//
// User-level API (experimental - may be deprecated anytime)
//

/**
 * @defgroup OCRExt OCR extensions/experimental APIs
 *
 * @brief These APIs are experimental or extensions that
 * are not necessarily supported on all platforms. They are
 * subject to change
 *
 * @{
 */
/**
 * @defgroup OCRExtAffinity Affinity extension
 *
 * @brief This extension is primarily used for the
 * distributed implementation of OCR to specify better
 * placement for EDTs and data blocks
 *
 * @{
 */

/**
 * @brief Types of affinities
 *
 * You can query for the affinities corresponding to
 * policy domains as well as for your own affinities
 */
typedef enum {
    AFFINITY_CURRENT, /**< Affinities of the current EDT */
    AFFINITY_PD, /**< Affinities of the policy domains. You can then
                  * affinitize EDTs and data blocks to these affinities
                  * in the creation calls */
    AFFINITY_PD_MASTER, /**< Runtime reserved (do not use) */
    AFFINITY_GUID /**< Affinities of a GUID */
} ocrAffinityKind;

/**
 * @brief Returns a count of affinity GUIDs of a particular kind.
 *
 * @param[in] kind      The affinity kind to query for. See #ocrAffinityKind
 * @param[out] count    Count of affinity GUIDs of that kind in the system
 * @return a status code
 *      - 0: successful
 */
u8 ocrAffinityCount(ocrAffinityKind kind, u64 * count);

/**
 * @brief Gets the affinity GUIDs of a particular kind. The 'affinities' array
 * must have been previously allocated and big enough to contain 'count'
 * GUIDs.
 *
 * @param[in] kind             The affinity kind to query for. See #ocrAffinityKind
 * @param[in,out] count        As input, requested number of elements; as output
 *                             the actual number of elements returned
 * @param[out] affinities      Affinity GUID array
 * @return a status code
 *      - 0: successful
 */
u8 ocrAffinityGet(ocrAffinityKind kind, u64 * count, ocrGuid_t * affinities);

/**
 * @brief Gets the affinity GUIDs of a particular kind using an index.
 *
 * @param[in] kind             The affinity kind to query for. See #ocrAffinityKind
 * @param[in] idx              The index of the GUID to query the 'i-th' elements of
 *                              the array that would be returned by 'ocrAffinityGet'
 * @param[out] affinities      Requested Affinity GUID (single value)
 * @return a status code
 *      - 0: successful
 *      - OCR_EINVAL if the index parameter is out of bounds
 */
u8 ocrAffinityGetAt(ocrAffinityKind kind, u64 idx, ocrGuid_t * affinity);

/**
 * @brief Returns an affinity the currently executing EDT is affinitized to
 *
 * An EDT may have multiple affinities. The programmer should rely on
 * ocrAffinityCount() and ocrAffinityGet() to query all affinities.
 *
 * @param[out] affinity    One affinity GUID for the currently executing EDT
 * @return a status code
 *      - 0: successful
 */
u8 ocrAffinityGetCurrent(ocrGuid_t * affinity);

/**
 * Get affinity GUIDs of a particular GUID currently belongs to.
 *
 * @param[in] guid             The GUID to query the affinity
 * @param[in,out] count        In: Requested number of element, Out: Actual element returned
 * @param[out] affinities      Affinity guid array, call-site allocated.
 * @return 0 on success or an error code on failure
 */
u8 ocrAffinityQuery(ocrGuid_t guid, u64 * count, ocrGuid_t * affinities);

/**
 * @brief Converts an affinity to a value suitable for ocrSetHintValue()
 */
u64 ocrAffinityToHintValue(ocrGuid_t affinity);

#ifdef __cplusplus
}
#endif
/**
 * @}
 * @}
 */
#endif /* ENABLE_EXTENSION_AFFINITY */

#endif /* __OCR_AFFINITY_H__ */

