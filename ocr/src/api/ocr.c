/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-sal.h"
#include "ocr-types.h"

#define DEBUG_TYPE API

static void ocrShutdownInternal(u8 errorCode) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER ocrShutdown()\n");
    ocrPolicyDomain_t *pd = NULL;
    PD_MSG_STACK(msg);
    ocrPolicyMsg_t * msgPtr = &msg;
    getCurrentEnv(&pd, NULL, NULL, msgPtr);
#define PD_MSG msgPtr
#define PD_TYPE PD_MSG_MGT_RL_NOTIFY
    msgPtr->type = PD_MSG_MGT_RL_NOTIFY | PD_MSG_REQUEST;
    PD_MSG_FIELD_I(runlevel) = RL_COMPUTE_OK;
    PD_MSG_FIELD_I(properties) = RL_REQUEST | RL_BARRIER | RL_TEAR_DOWN;
    PD_MSG_FIELD_I(errorCode) = errorCode;
    u8 returnCode = pd->fcts.processMessage(pd, msgPtr, true);
    ASSERT((returnCode == 0));
#undef PD_MSG
#undef PD_TYPE
}

void ocrShutdown() {
    ocrShutdownInternal(0);
}

void ocrAbort(u8 errorCode) {
    ocrShutdownInternal(errorCode);
}

u64 getArgc(void* dbPtr) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER getArgc(dbPtr=0x%lx)\n", dbPtr);
    DPRINTF(DEBUG_LVL_INFO, "EXIT getArgc -> %lu\n", ((u64*)dbPtr)[0]);
    return ((u64*)dbPtr)[0];

}

char* getArgv(void* dbPtr, u64 count) {
    DPRINTF(DEBUG_LVL_INFO, "ENTER getArgv(dbPtr=0x%lx, count=%lu)\n", dbPtr, count);
    u64* dbPtrAsU64 = (u64*)dbPtr;
    ASSERT(count < dbPtrAsU64[0]); // We can't ask for more args than total
    u64 offset = dbPtrAsU64[1 + count];
    DPRINTF(DEBUG_LVL_INFO, "EXIT getArgv -> %s\n", ((char*)dbPtr) + offset);
    return ((char*)dbPtr) + offset;
}
