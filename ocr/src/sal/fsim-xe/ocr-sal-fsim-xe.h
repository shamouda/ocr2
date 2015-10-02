/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_SAL_FSIM_XE_H__
#define __OCR_SAL_FSIM_XE_H__

#include "ocr-hal.h"
#include "xe-abi.h"

/**
 * @brief Function to drive the runlevel changes on boot-up as well
 * as the runlevel changes on tear-down
 *
 * This function will be called by tgkrnl to start the PD for the XE
 *
 * @param[in] pd    Pointer to the policy domain to start
 */
void salPdDriver(void* pd);

extern u32 salPause(bool isBlocking);

extern ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags);

extern void salResume(u32 flag);
#define sal_abort() hal_abort()

#define sal_exit(x) hal_exit(x)

#define sal_assert(x, fn, ln) do { if(!(x)) {                           \
            __asm__ __volatile__ __attribute__((noreturn)) (            \
                "lea r507, r507\n\t"                                    \
                "alarm %2\n\t"                                          \
                :                                                       \
                : "{r507}" (fn), "{r506}" (ln), "L" (XE_ASSERT_ERROR)   \
                : "r507");                                              \
        } } while(0)

#define sal_print(msg, len) __asm__ __volatile__(                   \
        "lea r506, r506\n\t"                                        \
        "alarm %2\n\t"                                              \
        :                                                           \
        : "{r506}" (msg), "{r507}" (len), "L" (XE_CONOUT_ALARM)     \
        : "r506")

#endif /* __OCR_SAL_FSIM_XE_H__ */
