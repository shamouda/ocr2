/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_RUNTIME_ITF_H_
#define OCR_RUNTIME_ITF_H_

#warning Using ocr-runtime-itf.h may not be supported on all platforms

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
 *  @brief Get the currently executing worker guid.
 *  Note: exposed as a convenience to runtime implementors, may be deprecated anytime.
 **/
ocrGuid_t ocrCurrentWorkerGuid();

/**
 *  @brief Inform the OCR runtime the currently executing thread is logically blocked
 *  Note: experimental feature for legacy code, may be deprecated anytime.
 **/
u8 ocrInformLegacyCodeBlocking();

/**
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* OCR_RUNTIME_ITF_H_ */
