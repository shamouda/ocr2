/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr.h"

/**
 * DESC: Test ELS set/get
 */

// Only tested when OCR runtime interface is available
#ifdef ENABLE_EXTENSION_RTITF

#include "extensions/ocr-runtime-itf.h"

// OCR must be SPECIFICALLY built with ELS support for this test to work
#ifndef TEST_OCR_ELS
#define TEST_OCR_ELS 0
#endif

#define ELS_OFFSET 0

void someUserFunction() {
    int * k = (int *) ocrElsUserGet(ELS_OFFSET);
    ASSERT(*((int *)k) == 42);
}

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    int *k;
    ocrGuid_t dbGuid;
    ocrDbCreate(&dbGuid,(void **) &k, sizeof(int), DB_PROP_NONE, NULL_GUID, NO_ALLOC);
    *k = 42;
    if (TEST_OCR_ELS) {
        ocrElsUserSet(ELS_OFFSET, (u64) k);
        someUserFunction();
    }
    ocrShutdown();
    return NULL_GUID;
}


#else

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    ocrShutdown();
    return NULL_GUID;
}

#endif
