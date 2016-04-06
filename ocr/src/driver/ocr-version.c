/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#include "ocr-types.h"
#include "ocr-version.h"

u32 ocrVersionExtractMajor(u8 *version)
{
    u32 ret = 0;
    u8 *ptr = version;
    while (ptr && ('0' <= *ptr) && ('9' >= *ptr)) {
        ret = 10*ret + ((*ptr) - '0');
        ptr++;
    }
    return ret;
}

u32 ocrVersionExtractMinor(u8 *version)
{
    u8 *ptr = version;
    /* Skip a period */
    while (ptr && ((*ptr) != '.')) ptr++;
    ptr++;
    return ocrVersionExtractMajor(ptr);
}

u32 ocrVersionExtractPatch(u8 *version)
{
    u8 *ptr = version;
    /* Skip 2 periods */
    while (ptr && ((*ptr) != '.')) ptr++;
    ptr++;
    while (ptr && ((*ptr) != '.')) ptr++;
    ptr++;
    return ocrVersionExtractMajor(ptr);
}

u64 ocrVersionExtensionBitmap(void)
{
    u64 retval = 0;

#ifdef ENABLE_EXTENSION_LEGACY
    retval |= OCR_VERSION_LEGACY_BIT;
#endif

#ifdef ENABLE_EXTENSION_AFFINITY
    retval |= OCR_VERSION_AFFINITY_BIT;
#endif

#ifdef ENABLE_EXTENSION_PARAMS_EVT
    retval |= OCR_VERSION_PARAMS_EVT_BIT;
#endif

#ifdef ENABLE_EXTENSION_COUNTED_EVT
    retval |= OCR_VERSION_COUNTED_EVT_BIT;
#endif

#ifdef ENABLE_EXTENSION_RTITF
    retval |= OCR_VERSION_RTITF_BIT;
#endif

#ifdef ENABLE_EXTENSION_PAUSE
    retval |= OCR_VERSION_PAUSE_BIT;
#endif

#ifdef ENABLE_EXTENSION_LABELING
    retval |= OCR_VERSION_LABELING_BIT;
#endif

    return retval;
}
