/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_SAL_LINUX_H__
#define __OCR_SAL_LINUX_H__

#include "ocr-hal.h"

#include <assert.h>
#include <stdio.h>

#define sal_abort()   hal_abort()

#define sal_exit(x)   hal_exit(x)

#define sal_assert(x, f, l)  assert(x)

#define sal_printf(fmt, ...)   printf(fmt, __VA_ARGS__)

#define sal_snprintf(buf, fmt, ...)   snprintf(buf, fmt, __VA_ARGS__)

#endif /* __OCR_SAL_LINUX_H__ */
