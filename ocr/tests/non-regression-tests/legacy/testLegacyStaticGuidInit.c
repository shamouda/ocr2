/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */
#include "ocr.h"

static const ocrGuid_t myNullGuid          = NULL_GUID_INITIALIZER;
static const ocrGuid_t myErrorGuid         = ERROR_GUID_INITIALIZER;
static const ocrGuid_t myUninitializedGuid = UNINITIALIZED_GUID_INITIALIZER;

ocrGuid_t mainEdt(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    PRINTF(GUIDF" "GUIDF" "GUIDF"\n",
           GUIDA(myNullGuid), GUIDA(myErrorGuid), GUIDA(myUninitializedGuid));
    ocrShutdown();
    return NULL_GUID;
}
