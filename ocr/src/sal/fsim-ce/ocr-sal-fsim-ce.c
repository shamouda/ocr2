#include "ocr-config.h"
#if defined(ENABLE_POLICY_DOMAIN_CE) && defined(ENABLE_COMP_PLATFORM_FSIM)

#include "debug.h"
#include "ocr-types.h"

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

#endif
