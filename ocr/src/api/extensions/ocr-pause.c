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
   u32 ret;
    do{
        ret = salPause();
        if(ret) break;
    }while(isBlocking);
    return ret;
}

//Note - Only queries runnable EDTs currently as a base case
//TODO: Expand to include specific query types.
void ocrQuery(u32 flag){
    salQuery(flag);
}

void ocrResume(u32 flag){
    salResume(flag);
}

#endif /* ENABLE_EXTENSION_PAUSE */

