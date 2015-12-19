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

//Fault simulation codes
typedef enum {
    OCR_FAULT_SIM_FREQUENCY,
    OCR_FAULT_SIM_MAX,
} ocrFaultSimCode_t;

/**
 * @brief Injects a simulated fault into the runtime
 *
 * @param[in] code : Simulated fault code
 * @param[in] faultData : Fault data associated with fault
 *
 **/
u8 ocrInjectFault(ocrFaultSimCode_t code, void *faultData);

#ifdef __cplusplus
}
#endif

#endif /* ENABLE_RESILIENCY_NULL */
#endif /* __OCR_RESILIENCY_SIM_H__ */
