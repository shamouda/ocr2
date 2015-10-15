#include "ocr-config.h"
#ifdef SAL_FSIM_CE

#include "debug.h"
#include "ocr-types.h"

#include "mmio-table.h"
#include "rmd-map.h"

#define DEBUG_TYPE API
/* NOTE: Below functions are placeholders for platform independence.
 *       Currently no functionality on tg.
 */

u32 salPause(bool isBlocking){
    DPRINTF(DEBUG_LVL_VERB, "ocrPause/ocrQuery/ocrResume not yet supported on tg\n");
    return 1;
}

ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){
     return NULL_GUID;
}

void salResume(u32 flag){
     return;

}

u64 salGetTime(void){
    u64 cycles = 0;
#if !defined(ENABLE_BUILDER_ONLY)
    cycles = rmd_ld64(CE_MSR_BASE + CYCLE_COUNTER * sizeof(u64));
#endif
    return cycles;
}

#endif
