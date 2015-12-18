/*
 * @brief OCR APIs for simulating runtime faults
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_RESILIENCY_SIM_H__
#define __OCR_RESILIENCY_SIM_H__

#ifdef ENABLE_RESILIENCY_NULL
#ifdef __cplusplus
extern "C" {
#endif

#include "ocr-types.h"

/**
 * @brief Injects a simulated FREQUENCY fault into the runtime
 *
 *
 * @param[in] newFreq new frequency alue detected (mHz)
 *
 **/
u8 ocrInjectFault(u64 newFreq);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_RESILIENCY_NULL */
#endif /* __OCR_RESILIENCY_SIM_H__ */
