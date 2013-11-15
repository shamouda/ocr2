/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_RUNTIME_ITF_H_
#define OCR_RUNTIME_ITF_H_
#ifdef __cplusplus
extern "C" {
#endif
/**
 * @defgroup OCRRuntimeItf Interface for runtimes built on top of OCR
 * @brief Defines additional API for runtime implementers
 *
 * @warning These APIs are not fully supported at this time
 * and should be used with caution

 * @{
 **/

#include "ocr-types.h"

/**
 *  @brief Get @ offset in the currently running edt's local storage
 *
 *  @param offset Offset in the ELS to fetch
 *  @return NULL_GUID if there's no ELS support.
 *  @warning Must be called from within an EDT code.
 **/
ocrGuid_t ocrElsUserGet(u8 offset);

/**
 *  @brief Set data @ offset in the currently running edt's local storage
 *  @param offset Offset in the ELS to set
 *  @param data   Value to write at that offset
 *
 *  @note No-op if there's no ELS support
 **/
void ocrElsUserSet(u8 offset, ocrGuid_t data);

/**
 *  @brief Get the currently executing edt.
 *  @return NULL_GUID if there is no EDT running.
 **/
ocrGuid_t currentEdtUserGet();

/**
 *  @brief Get the number of workers the runtime currently uses
 *  Note: exposed as a convenience to runtime implementors, may be deprecated anytime.
 **/
u64 ocrNbWorkers();

/**
 *  @brief Get the currently executing worker 'id'
 *  Note: exposed as a convenience to runtime implementors, may be deprecated anytime.
 **/
u64 ocrCurrentWorkerId();


/*********************************************/
/* Adaptive API                              */  
/* Warning !! API to be deprecated anytime   */
/*********************************************/

/**
 *  @brief Throttle the runtime by some metric
 **/
void ocrThrottle(int metric);

/**
 *  @brief Register a user-defined adaptation function
 **/
void ocrRegisterAdaptFct(u64 (*adaptObjectiveFct)(int));

/**
 *  @brief Get the current adaptation objective
 **/
u64 ocrGetAdaptObjective();

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* OCR_RUNTIME_ITF_H_ */
