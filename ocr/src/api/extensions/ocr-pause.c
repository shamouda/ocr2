/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_EXTENSION_PAUSE
#include "extensions/ocr-pause.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "ocr-sal.h"

u32 ocrPause(bool isBlocking){
    return salPause(isBlocking);
}

ocrGuid_t ocrQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){
    ocrGuid_t ret = salQuery(query, guid, result, size, flags);
    return ret;
}

void ocrResume(u32 flag){
    salResume(flag);
}

#endif /* ENABLE_EXTENSION_PAUSE */

