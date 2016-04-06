/**
* @brief Top level OCR legacy support. Include this file if you
* want to call OCR from sequential code for example
**/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_LABELING_H__
#define __OCR_LABELING_H__

#ifdef ENABLE_EXTENSION_LABELING

#ifdef __cplusplus
extern "C" {
#endif

// TODO: This may be valid for other things. Move into main API?
// TODO: Needs to be in sync with ocr-guid-kind.h for now. This is obviously not ideal
// See BUG #561
typedef enum {
    GUID_USER_NONE = 0,
    GUID_USER_DB = 2,
    GUID_USER_EDT = 3,
    GUID_USER_EDT_TEMPLATE = 4,
    GUID_USER_EVENT_ONCE = 17,
#ifdef ENABLE_EXTENSION_COUNTED_EVT
    GUID_USER_EVENT_COUNTED = 18,
#endif
    GUID_USER_EVENT_IDEM = 19,
    GUID_USER_EVENT_STICKY = 21,
    GUID_USER_EVENT_LATCH = 22
} ocrGuidUserKind;

/**
 * @brief Creates a map function
 *
 * This function creates a GUID mapping function which is able to associate a unique GUID to an input
 * tuple. This function will reserve a range of GUIDs based on the number of GUIDs needed. GUID
 * reservation does not mean that the GUIDs are allocated in any way just that they won't be used
 * by the runtime for any other purpose.
 *
 * @param[out] mapGuid            Returns the GUID for this map function
 * @param[in] numParams           Number of values in 'params'
 * @param[in] mapFunc             The mapping function. This function should be bijective
 *                                from the input tuple space to the GUID space restricted
 *                                by startGuid and skipGuid. This function takes as input, the
 *                                first GUID in the reserved range, the value of skipGuid,
 *                                user-defined parameters passed during the creation of the
 *                                map-function (a sort of memoization) and
 *                                its input tuple and outputs a GUID
 * @param[in] params              Parameters for the map function (memoized)
 * @param[in] numberGuid          Total number of GUIDs to reserve
 * @param[in] kind                Kind of the GUIDs stored in this map
 * @return 0 on success or a non-zero error code
 */
u8 ocrGuidMapCreate(ocrGuid_t *mapGuid, u32 numParams, ocrGuid_t (*mapFunc)(
                        ocrGuid_t startGuid, u64 skipGuid, s64* params, s64* tuple),
                    s64* params, u64 numberGuid, ocrGuidUserKind kind);

/**
 * @brief Creates a range of GUID (similar to a map but without the map function
 *
 * This function is similar to ocrGuidMapCreate() except that you do not pass the
 * mapping function. To "map" a tuple to a GUID, the user will first be responsible
 * for converting the tuple to a sequence number between 0 and numberGuid-1 and the
 * runtime will then automatically map this number to a GUID.
 *
 * @param[out] rangeGuid         Returns the GUID of this range
 * @param[in] numberGuid         Total number of GUIDs to reserve
 * @param[in] kind               Kind of the GUIDs stored in this range
 * @return 0 on success or a non-zero error code
 */
u8 ocrGuidRangeCreate(ocrGuid_t *rangeGuid, u64 numberGuid, ocrGuidUserKind kind);

/**
 * @brief Destroys a map function
 *
 * This function causes the map function to be destroyed.
 * This does not affect the GUIDs that have been created with
 * this map but will un-reserve the ones that have not been used
 * (note that guidUnreserve does just that -- it ignores valid GUIDs)
 *
 * This can also be used for ranges created by ocrGuidRangeCreate
 *
 * @param[in] mapGuid            GUID of the map to destroy
 * @return 0 on success or a non-zero error code
 */
u8 ocrGuidMapDestroy(ocrGuid_t mapGuid);

/**
 * @brief Defines the default GUID map
 *
 * This function defines the global GUID map. It should be called
 * one per program preferably early on. Using the default map before it is defined
 * will result in an error.
 *
 * @param[in] numParams           Number of parameters in params
 * @param[in] mapFunc             The mapping function. This function should be bijective
 *                                from the input tuple space to the GUID space restricted
 *                                by startGuid and skipGuid. This function takes as input, the
 *                                first GUID in the reserved range, the value of skipGuid,
 *                                user-defined parameters passed during the creation of the
 *                                map-function (a sort of memorization) and
 *                                its input tuple and outputs a GUID
 * @param[in] params              Parameters for the map function (memoized)
 * @param[in] numberGuid          Total number of GUIDs to reserve
 * @param[in] kind                Kind of the GUIDs stored in this map
 * @return 0 on success or a non-zero error code
 */
u8 ocrGuidMapSetDefaultMap(u32 numParams, ocrGuid_t (*mapFunc)(
                               ocrGuid_t startGuid, u64 skipGuid, s64* params, s64* tuple),
                           s64* params, u64 numberGuid, ocrGuidUserKind kind);

/**
 * @brief Convert a user "label" (tuple) to a GUID
 *
 * This function will return a GUID for the input tuple provided.
 *
 * @param[out] outGuid  Returns the value of the GUID for the given tuple
 * @param[in] mapGuid   GUID for the map function to use (created by
 *                      ocrGuidMapCreate()) or NULL_GUID to use the default map
 * @param[in] tuple     Tuple to convert to a GUID
 * @return 0 on success or a non-zero error code:
 *     - OCR_EINVAL if mapGuid does not refer to a valid map
 *
 * @note This function makes no claim about the existence of an object for the GUID returned.
 * It is, in effect, simply a call to map the input tuple. You can use ocrGuidKind to
 * determine the validity of the GUID returned
 */
u8 ocrGuidFromLabel(ocrGuid_t *outGuid, ocrGuid_t mapGuid, s64* tuple);

/**
 * @brief Convert a user index to a GUID
 *
 * This function is similar to ocrGuidFromLabel() except that ocrGuidFromLabel()
 * relies on a map whereas this function relies on a range
 *
 * @param[out] outGuid  Returns the value of the GUID for the given index
 * @param[in] rangeGuid GUID of the range to use (created by ocrGuidRangeCreate())
 * @param[in] idx       Inde to convert
 * @return 0 on success or a non-zero error code:
 *     - OCR_EINVAL if rangeGuid does not refer to a valid range
 */
u8 ocrGuidFromIndex(ocrGuid_t *outGuid, ocrGuid_t rangeGuid, u64 idx);

/**
 * @brief Determines the type of a GUID and whether or not it is valid
 *
 * This function will return the kind of a GUID. It will return OCR_GUID_NONE
 * if the GUID is not a valid GUID.
 *
 * @param[out] outKind    Returns the type of the GUID
 * @param[in] guid        Input GUID
 * @return 0 on success or a non-zero error code
 */
u8 ocrGetGuidKind(ocrGuidUserKind *outKind, ocrGuid_t guid);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_EXTENSION_LABELING */
#endif /* __OCR_LABELING_H__ */
