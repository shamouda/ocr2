/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_SAL_FSIM_CE_H__
#define __OCR_SAL_FSIM_CE_H__

#include "ocr-hal.h"

/**
 * @brief Function to drive the runlevel changes on boot-up as well
 * as the runlevel changes on tear-down
 *
 * This function will be called by tgkrnl to start the PD for the CE
 *
 * @param[in] pd    Pointer to the policy domain to start
 */
void salPdDriver(void* pd);

extern u32 salPause(bool isBlocking);

extern ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags);

extern void salResume(u32 flag);

#define sal_abort()   hal_abort()

#define sal_exit(x)   hal_exit(x)

#define sal_assert(x, fn, ln)   do { if(!(x)) {                         \
            PRINTF("ASSERT FAILURE: CE at line %"PRId32" in '%s'\n", (int)(ln), fn); \
            hal_abort();                                                \
        } } while(0)

#define sal_print(msg, len) __asm__ __volatile__("int $0xFF\n\t" : : "a" (msg))

#endif /* __OCR_SAL_FSIM_CE_H__ */
