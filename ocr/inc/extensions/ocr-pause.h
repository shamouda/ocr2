/*
 * @brief OCR APIs for pause, query, and resume of runtime
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_PAUSE_H__
#define __OCR_PAUSE_H__
#ifdef ENABLE_EXTENSION_PAUSE

#ifdef __cplusplus
extern "C" {
#endif

#include "ocr-types.h"

/**
 * @brief Halts forward progress of all workers to allow querying of current
 * runtime contents.
 *
 * @This call will cause workers to suspend all future EDT execution
 * until ocrResume() is called, where execution will then continue normally
 *
 * @param isBlocking   Flag to indicate whether to block to avoid worker pause
 * contention
 **/
u32 ocrPause(bool isBlocking);

/**
 * @brief queries the contents of runtime while runtime is paused
 *
 * @note If ocrQuery() gets called while runtime is not paused it will be
 * ignored.
 *
 * @param flag   Used to indicate whether or not query should be executed
 * depending on whether pause was succesful
 *
 **/
void ocrQuery(u32 flag);

/**
 * @brief Continues execution of a paused OCR program.
 *
 * @This call will restart suspended workers
 *
 * @note If ocrResume() is called while a paused runtime state is not detected
 * the call will effectively be ignored.
 *
 * @param flag   Used to indicate whether or not resume hook should be executed
 * depending on whether pause was succesful
 **/
void ocrResume(u32 flag);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_EXTENSION_PAUSE */

#endif /* __OCR_PAUSE_H__ */
