/**
 * @brief Top level OCR legacy support. Include this file if you
 * want to call OCR from sequential code for example
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_LEGACY_H__
#define __OCR_LEGACY_H__

#warning Using ocr-legacy.h may not be supported on all platforms
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @ingroup OCRExt
 * @{
 */
/**
 * @defgroup OCRExtLegacy OCR used from legacy code
 * @brief API to use OCR from legacy programming models
 *
 *
 * @{
 **/

/**
 * @brief Waits on the satisfaction of an event
 *
 * @warning The event waited on must be a persistent event (in other
 * words, not a LATCH or ONCE event).
 *
 * @warning This call may be slow and inefficient. It is meant
 * only to call OCR from sequential code and should be avoided in a
 * pure OCR program
 *
 * @param[in] outputEvent   GUID of the event to wait on
 *
 * @return The GUID of the data block on the post-slot of the
 * waited-on event
 */
ocrGuid_t ocrWait(ocrGuid_t outputEvent);

/**
 * @brief Bring up the OCR runtime
 *
 * This function needs to be called to bring up the runtime. It
 * should be called once for each runtime that needs to be brought
 * up.
 *
 * @param[out] legacyContext Returns the ID of the legacy context
 *                           created
 * @param[in]  ocrConfig     Configuration parameters to bring up the
 *                           runtime
 */
void ocrInitLegacy(ocrGuid_t *legacyContext, ocrConfig_t * ocrConfig);

/**
 * @brief Prepares to tear down the OCR runtime
 *
 * This call prepares the runtime to be torn-down. This call
 * will only return after the OCR program completes (ie: after
 * the program calls ocrShutdown()).
 *
 * @param[in] legacyContext Legacy context to finalize. This
 *                          value is obtained from ocrInit()
 * @return the status code of the OCR program:
 *      - 0: clean shutdown, no errors
 *      - non-zero: user provided error code to ocrAbort()
 */
u8 ocrFinalizeLegacy(ocrGuid_t legacyContext);


/**
 * @brief Launch an OCR "procedure" from a legacy sequential section of code
 *
 * This call requires the sequential code to have already called ocrInit().
 * This call will *NOT* trigger ocrFinalize(). This call is non-blocking.
 *
 * @param[out] handle             Handle to use in ocrBlockProgressUntilComplete
 * @param[in]  finishEdtTemplate  Template of the EDT to execute. This must be
 *                                the template of a finish EDT
 * @param[in]  paramc             Number of parameters to pass to the EDT
 * @param[in]  paramv             Array of parameters to pass. Will be
 *                                passed by value
 * @param[in]  depc               Number of data-blocks to pass in
 * @param[in]  depv               GUID for these data-blocks (created using
 *                                ocrDbCreate())
 * @param[in]  legacyContext      Legacy context this is called from. This needs
 *                                to be remembered from ocrInit().
 * @return 0 on success or an error code on failure (from ocr-errors.h)
 */

u8 ocrSpawnOCR(ocrGuid_t* handle, ocrGuid_t finishEdtTemplate, u64 paramc, u64* paramv,
	 u64 depc, ocrGuid_t depv, ocrGuid_t legacyContext);

/**
 * @brief Waits for the result of an OCR procedure launched with ocrSpawnOCR
 *
 * This call requires the sequential code to have already called ocrInit().
 * This call will *NOT* trigger a call to ocrFinalize(). This call is (obviously)
 * blocking.
 * @param[in]  handle    Event to wait on (usually 'handle' from ocrSpawnOCR)
 * @param[out] guid      GUID of the data-block returned.
 * @param[out] result    Returns a void* pointer to the data-block
 * @param[out] size      Returns the size of the data-block
 * @return 0 on success or an error code on failure (from ocr-errors.h)
 */

u8 ocrLegacyBlockProgress(ocrGuid_t handle, ocrGuid_t *guid, void **result, u64* size);

/**
 * @}
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* __OCR_LEGACY_H__ */
