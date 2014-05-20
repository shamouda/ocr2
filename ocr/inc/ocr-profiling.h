/**
 * @brief Profiling support
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __OCR_PROFILING_H__
#define __OCR_PROFILING_H__

#include <ocr-types.h>

struct _profileStruct {
    char *fname;
    u8 numIcCoeff;
    u8 numFpCoeff;
    u8 numRdCoeff;
    u8 numWrCoeff;
    double icVar;
    double fpVar;
    double rdVar;
    double wrVar;
    double *icCoeff;
    double *fpCoeff;
    double *rdCoeff;
    double *wrCoeff;
    u64 flags;
} profileStruct;

#endif /* __OCR_HAL_H__ */
