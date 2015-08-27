/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <stdlib.h>
#include <string.h>
#include "ocr.h"

/**
 * DESC: Test Argc/Argv calls to read input for mainEdt
 */

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Test arguments are : number of args, string size, string
    u32 argc = getArgc(depv[0].ptr);
    ASSERT(argc == 4);

    int remArgs = atoi(getArgv(depv[0].ptr, 1));
    ASSERT(remArgs == 2);
    int strSize = atoi(getArgv(depv[0].ptr, 2));
    ASSERT(strSize == 4);
    char* str = getArgv(depv[0].ptr, 3);
    ASSERT(strlen(str) == 4);
    ASSERT(strcmp("abcd", str) == 0);
    ocrShutdown();
    return NULL_GUID;
}