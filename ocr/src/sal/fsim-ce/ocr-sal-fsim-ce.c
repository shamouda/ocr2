#include "ocr-config.h"
#ifdef SAL_FSIM_CE

#include "debug.h"
#include "ocr-types.h"

#include "xstg-arch.h"
#include "mmio-table.h"

#define DEBUG_TYPE SAL

void salPdDriver(void* pdVoid) {
    ocrPolicyDomain_t *pd = (ocrPolicyDomain_t*)pdVoid;
    DPRINTF(DEBUG_LVL_INFO, "CE PD Driver for pd @ %p\n", pd);

    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_CONFIG_PARSE, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with CONFIG_PARSE\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_NETWORK_OK, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with NETWORK_OK\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_PD_OK, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with PD_OK\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_MEMORY_OK, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with MEMORY_OK\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_GUID_OK, RL_REQUEST | RL_BARRIER
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with GUID_OK\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_COMPUTE_OK, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);
    DPRINTF(DEBUG_LVL_VERB, "Done with COMPUTE_OK\n");
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_USER_OK, RL_REQUEST | RL_ASYNC
                                          | RL_BRING_UP | RL_NODE_MASTER), ==, 0);

    // When we come back here, we continue bring down from GUID_OK. The switchRunlevel
    // function takes care of coming out of USER_OK and through COMPUTE_OK as well. This makes
    // it a bit easier as both barriers are already traversed when we get here on
    // shutdown so we don't have to worry about that.
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_GUID_OK, RL_REQUEST | RL_ASYNC | RL_TEAR_DOWN | RL_NODE_MASTER),
                  ==, 0);
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_MEMORY_OK, RL_REQUEST | RL_ASYNC | RL_TEAR_DOWN | RL_NODE_MASTER),
                  ==, 0);

    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_PD_OK, RL_REQUEST | RL_ASYNC | RL_TEAR_DOWN | RL_NODE_MASTER),
                  ==, 0);
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_NETWORK_OK, RL_REQUEST | RL_ASYNC | RL_TEAR_DOWN | RL_NODE_MASTER),
                  ==, 0);
    RESULT_ASSERT(pd->fcts.switchRunlevel(pd, RL_CONFIG_PARSE, RL_REQUEST | RL_ASYNC | RL_TEAR_DOWN | RL_NODE_MASTER),
                  ==, 0);
    return;
}

/* NOTE: Below functions are placeholders for platform independence.
 *       Currently no functionality on tg.
 */

u32 salPause(bool isBlocking) {
    DPRINTF(DEBUG_LVL_VERB, "ocrPause/ocrQuery/ocrResume not yet supported on tg\n");
    return 1;
}

ocrGuid_t salQuery(ocrQueryType_t query, ocrGuid_t guid, void **result, u32 *size, u8 flags){
     return NULL_GUID;
}

void salResume(u32 flag) {
     return;

}

u64 salGetTime(void){
    u64 cycles = 0;
#if !defined(ENABLE_BUILDER_ONLY)
    cycles = tg_ld64((u64*)(AR_MSR_BASE + GLOBAL_TIME_STAMP_COUNTER * sizeof(u64)));
#endif
    return cycles;
}

#endif
