#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_XE

#include "debug.h"
#include "ocr-types.h"

#define DEBUG_TYPE API
/* NOTE: Below functions are placeholders for platform independence.
 *       Currently no functionality on tg.
 */

u32 salPause(void){
    DPRINTF(DEBUG_LVL_VERB, "ocrPause/ocrQuery/ocrResume not yet supported on tg\n");
    return 1;
}

void salQuery(u32 flag){
     return;
}

void salResume(u32 flag){
     return;

}

#endif /* ENABLE_POLICY_DOMAIN_XE */
