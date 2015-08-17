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

#ifdef ENABLE_EXTENSION_LEGACY

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
 * @brief Data-structure containing the configuration
 * parameters for the runtime
 *
 * The contents of this struct can be filled in by calling
 * ocrParseArgs() or by setting them manually. The former
 * method is strongly recommended.
 *
 */
typedef struct _ocrConfig_t {
    int userArgc;          /**< Application argc (after having stripped the OCR arguments) */
    char ** userArgv;      /**< Application argv (after having stripped the OCR arguments) */
    const char * iniFile;  /**< INI configuration file for the runtime */
} ocrConfig_t;

/**
 * @brief Parses the arguments passed to main and extracts the
 * relevant information to initialize OCR
 *
 * This should be called prior to ocrLegacyInit() to populate the
 * #ocrConfig_t variable needed by ocrLegacyInit().
 *
 * @param[in] argc           The number of elements in argv
 * @param[in] argv           Array of char * argumetns.
 * @param[in,out] ocrConfig  Pointer to an ocrConfig ocrParseArgs will populate. ocrConfig
 *                           needs to have already been allocated
 */
void ocrParseArgs(int argc, const char* argv[], ocrConfig_t * ocrConfig);

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
void ocrLegacyInit(ocrGuid_t *legacyContext, ocrConfig_t * ocrConfig);

/**
 * @brief Brings down the runtime (or prepares to do so)
 *
 * This call tears down the runtime (if runUntilShutdown is
 * false) or prepares to tear it down otherwise. This call
 * will only return after the runtime has been torn down. If
 * runUntilShutdown is true, this includes waiting for
 * ocrShutdown() to be called.
 *
 * @param[in] legacyContext    Legacy context to finalize. This
 *                             value is obtained from ocrLegacyInit()
 * @param[in] runUntilShutdown If true, the runtime will wait for ocrShutdown()
 *                             to be called to bring down the runtime. Otherwise,
 *                             this call tears down the runtime and assumes no more EDTs
 *                             are active/running. This latter case is typical in a legacy
 *                             application with sporadic calls out to OCR. The former case is
 *                             typical in a fully OCR-ized program
 * @return the status code of the OCR program:
 *      - 0: clean shutdown, no errors
 *      - non-zero: user provided error code to ocrAbort()
 */
u8 ocrLegacyFinalize(ocrGuid_t legacyContext, bool runUntilShutdown);

/**
 * @brief Launch an OCR "procedure" from a legacy sequential section of code
 *
 * This call requires the sequential code to have already called ocrLegacyInit().
 * This call will *NOT* trigger ocrLegacyFinalize(). This call is non-blocking and
 * is frequently used with ocrLegacyBlockProgress().
 *
 * @param[out] handle             Handle to use in ocrLegacyBlockProgress().
 *                                This is a persistent event which the user needs
 *                                to destroy using ocrEventDestroy() after it
 *                                has been waited on
 * @param[in]  finishEdtTemplate  Template of the EDT to execute. This must be
 *                                the template of a finish EDT
 * @param[in]  paramc             Number of parameters to pass to the EDT
 * @param[in]  paramv             Array of parameters to pass. Will be
 *                                passed by value
 * @param[in]  depc               Number of data-blocks to pass in. A minimum
 *                                of one data-block is required.
 * @param[in]  depv               GUID for these data-blocks (created using
 *                                ocrDbCreate()). Note that the content of these
 *                                data-blocks may change until ocrLegacyBlockProgress()
 *                                returns. Modifying these data-blocks between this
 *                                call and the return from ocrLegacyBlockProgress() may
 *                                result in a data-race.
 * @param[in]  legacyContext      Legacy context this is called from. This needs
 *                                to be remembered from ocrLegacyInit().
 * @note The GUID returned by 'handle' needs to be destroyed using ocrEventDestroy()
 * once it has been used in ocrLegacyBlockProgress().
 * @return 0 on success or an error code on failure (from ocr-errors.h)
 */

u8 ocrLegacySpawnOCR(ocrGuid_t* handle, ocrGuid_t finishEdtTemplate, u64 paramc, u64* paramv,
                     u64 depc, ocrGuid_t* depv, ocrGuid_t legacyContext);

/**
 * @brief Waits on the satisfaction of an event
 *
 * @warning The event waited on must be a persistent event (in other
 * words, not a LATCH or ONCE event).
 *
 * @param[in] outputEvent   GUID of the event to wait on
 *
 * @return The GUID of the data block on the post-slot of the
 * waited-on event
 * @deprecated This call is deprecated in favor of
 * ocrLegacyBlockProgress()
 */
ocrGuid_t ocrWait(ocrGuid_t outputEvent);

/**
 * @brief Waits for the result of an OCR procedure launched with ocrSpawnOCR
 *
 * This call requires the sequential code to have already called ocrLegacyInit().
 * This call will *NOT* trigger a call to ocrLegacyFinalize(). This call is (obviously)
 * blocking.
 *
 * @warning handle should be a persistent event (not a LATCH or ONCE event)
 * @param[in]  handle    Event to wait on (usually 'handle' from ocrSpawnOCR)
 * @param[out] guid      If non NULL, if this call succeeds, will contain the
 *                       GUID of the data-block returned.
 * @param[out] result    If non NULL, if this call succeeds, will contain
 *                       the void* to the data-block
 * @param[out] size      If non NULL, it this call succeeds, will contain the
 *                       size in bytes of the returned data-block
 * @param[in] properties Flags for the wait. Currently supported:
 *                           - LEGACY_PROP_NONE: Default behavior
 *                           - LEGACY_PROP_WAIT_FOR_CREATE: Wait for handle to be created if
 *                             it is not yet a valid event GUID
 * @return 0 on success or an error code on failure (from ocr-errors.h):
 *   - OCR_EINVAL: 'handle' does not point to a valid event and LEGACY_PROP_WAIT_FOR_CREATE
 *                 is not specified
 */

u8 ocrLegacyBlockProgress(ocrGuid_t handle, ocrGuid_t *guid, void **result, u64* size,
                          u16 properties);
// Temporary flag due to addition of properties.
// Will be eliminated
// See BUG #562
#define LEGACY_BLOCK_PROGRESS_5_ARGS 1
/**
 * @}
 * @}
 */
#ifdef __cplusplus
}
#endif

#endif /* ENABLE_EXTENSION_LEGACY */
#endif /* __OCR_LEGACY_H__ */
