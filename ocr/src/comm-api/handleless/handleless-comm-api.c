/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_API_HANDLELESS

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-worker.h"
#include "ocr-comm-platform.h"
#include "utils/ocr-utils.h"
#include "handleless-comm-api.h"

#define DEBUG_TYPE COMM_API
void handlelessCommDestructHandle(ocrMsgHandle_t *handle);

void handlelessCommDestruct (ocrCommApi_t * base) {
    // call destruct on child
    if(base->commPlatform) {
        base->commPlatform->fcts.destruct(base->commPlatform);
        base->commPlatform = NULL;
    }
    runtimeChunkFree((u64)base, PERSISTENT_CHUNK);
}

u8 handlelessCommSwitchRunlevel(ocrCommApi_t *self, ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                                phase_t phase, u32 properties, void (*callback)(ocrPolicyDomain_t*, u64), u64 val) {

    u8 toReturn = 0;

    // This is an inert module, we do not handle callbacks (caller needs to wait on us)
    ASSERT(callback == NULL);

    // Verify properties for this call
    ASSERT(!(properties & RL_FROM_MSG));

    if(properties & RL_BRING_UP) {
        toReturn |= self->commPlatform->fcts.switchRunlevel(self->commPlatform, PD, runlevel, phase,
                                                            properties, NULL, 0);
    }

    switch(runlevel) {
    case RL_CONFIG_PARSE:
        // On bring-up: Update PD->phasesPerRunlevel on phase 0
        // and check compatibility on phase 1
        break;
    case RL_NETWORK_OK:
        break;
    case RL_PD_OK:
        if(properties & RL_BRING_UP)
            self->pd = PD;
        break;
    case RL_MEMORY_OK:
        break;
    case RL_GUID_OK:
        break;
    case RL_COMPUTE_OK:
        break;
    case RL_USER_OK:
        break;
    default:
        // Unknown runlevel
        ASSERT(0);
    }

    if(properties & RL_TEAR_DOWN) {
        toReturn |= self->commPlatform->fcts.switchRunlevel(self->commPlatform, PD, runlevel, phase,
                                                            properties, NULL, 0);
    }
    return toReturn;
}

u8 handlelessInitHandle(ocrCommApi_t *self, ocrMsgHandle_t* handle) {
    handle->msg = handle->response = NULL;
    handle->status = HDL_NORMAL;
    handle->destruct = FUNC_ADDR(
        void (*)(ocrMsgHandle_t*), handlelessCommDestructHandle);
    handle->properties = 0;
    handle->commApi = self;
    return 0;
}

u8 handlelessCommSendMessage(ocrCommApi_t *self, ocrLocation_t target, ocrPolicyMsg_t *message,
                             ocrMsgHandle_t **handle, u32 properties) {
    u64 id;
    u8 retval;

    // If we have a handle, currently it should be a two way message
    // This may change in the future when we want to support queries on whether
    // a one way message was properly sent
    if(handle) {
        ASSERT(properties & TWOWAY_MSG_PROP);
        ASSERT(*handle); // We need to have a handle that is already allocated
        (*handle)->destruct = FUNC_ADDR(
            void (*)(ocrMsgHandle_t*), handlelessCommDestructHandle);
        // If persistent, remember where the message was
        if(properties & PERSIST_MSG_PROP)
            (*handle)->msg = message;

        // This needs to be here because the handle is initialized only
        // when polling directly. If it is used to send a message, it may
        // not be always initialized
        (*handle)->response = NULL;
        (*handle)->status = HDL_NORMAL;
        (*handle)->properties = 0;
        (*handle)->commApi = self;
    }

    retval = self->commPlatform->fcts.sendMessage(self->commPlatform, target, message, &id, properties, 0);
    if(retval != 0 && handle) {
        // We need the handle to actually exist
        ASSERT(*handle);
        (*handle)->status = HDL_SEND_ERR;
    }
    return retval;
}

u8 handlelessCommPollMessage(ocrCommApi_t *self, ocrMsgHandle_t **handle) {
    u8 retval;
    // Only works if handle is allocated. We only support
    // an allocated handle and currently return ANY message (ie: not the one
    // specifically for the handle). This is just because this Comm-API
    // implementation does not internally handle handles
    ASSERT(handle && *handle);

    // The handle should be valid
    ASSERT((*handle)->status == HDL_NORMAL);

    // If the in message was persistent, we can always re-use it
    // Pass that as a hint
    if((*handle)->msg) {
        (*handle)->response = (*handle)->msg;
    }

    retval = self->commPlatform->fcts.pollMessage(
                      self->commPlatform, &((*handle)->response), 0, NULL);
    if(retval == 0) {
        if((*handle)->response == (*handle)->msg) {
            // This means that the comm platform did *not* allocate the buffer itself
            (*handle)->properties = 0; // Indicates "do not free"
        } else {
            // This means that the comm platform did allocate the buffer itself
            (*handle)->properties = 1; // Indicates "do free"
        }
        (*handle)->status = HDL_RESPONSE_OK;
    } else {
        (*handle)->status = HDL_RECV_ERR;
    }
    return retval;
}

u8 handlelessCommWaitMessage(ocrCommApi_t *self, ocrMsgHandle_t **handle) {
    u8 retval;
    // Only works if handle is allocated
    ASSERT(handle && *handle);

    // The handle should be valid
    ASSERT((*handle)->status == HDL_NORMAL);

    // If the in message was persistent, we can always re-use it
    // Pass that as a hint
    if((*handle)->msg) {
        (*handle)->response = (*handle)->msg;
    }

    retval = self->commPlatform->fcts.waitMessage(
        self->commPlatform, &((*handle)->response), 0, NULL);
    if(retval == 0) {
        if((*handle)->response == (*handle)->msg) {
            // This means that the comm platform did *not* allocate the buffer itself
            (*handle)->properties = 0; // Indicates "do not free"
        } else {
            // This means that the comm platform did allocate the buffer itself
            (*handle)->properties = 1; // Indicates "do free"
        }
        (*handle)->status = HDL_RESPONSE_OK;
    } else {
        (*handle)->status = HDL_RECV_ERR;
    }
    return retval;
}

void handlelessCommDestructHandle(ocrMsgHandle_t *handle) {
    // We should have a handle
    ASSERT(handle);
    // We should have received a proper response
    ASSERT(handle->status == HDL_RESPONSE_OK);
    if(handle->properties == 1) {
        // Should have something to free
        ASSERT(handle->response);
        RESULT_ASSERT(handle->commApi->commPlatform->fcts.destructMessage(
                          handle->commApi->commPlatform, handle->response), ==, 0);
    }
    handle->msg = NULL;
    handle->response = NULL;
    handle->status = HDL_ERR;
    handle->properties = 0;
}

ocrCommApi_t* newCommApiHandleless(ocrCommApiFactory_t *factory,
                                   ocrParamList_t *perInstance) {

    ocrCommApiHandleless_t * commApiHandleless = (ocrCommApiHandleless_t*)
        runtimeChunkAlloc(sizeof(ocrCommApiHandleless_t), PERSISTENT_CHUNK);
    factory->initialize(factory, (ocrCommApi_t*) commApiHandleless, perInstance);

    return (ocrCommApi_t*)commApiHandleless;
}

/**
 * @brief Initialize an instance of comm-api handleless
 */
void initializeCommApiHandleless(ocrCommApiFactory_t * factory, ocrCommApi_t* self, ocrParamList_t * perInstance) {
    initializeCommApiOcr(factory, self, perInstance);
}

/******************************************************/
/* OCR COMM API HANDLELESS                            */
/******************************************************/

void destructCommApiFactoryHandleless(ocrCommApiFactory_t *factory) {
    runtimeChunkFree((u64)factory, NONPERSISTENT_CHUNK);
}

ocrCommApiFactory_t *newCommApiFactoryHandleless(ocrParamList_t *perType) {
    ocrCommApiFactory_t *base = (ocrCommApiFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCommApiFactoryHandleless_t), NONPERSISTENT_CHUNK);

    base->instantiate = newCommApiHandleless;
    base->initialize = initializeCommApiHandleless;
    base->destruct = destructCommApiFactoryHandleless;
    base->apiFcts.destruct = FUNC_ADDR(void (*)(ocrCommApi_t*), handlelessCommDestruct);
    base->apiFcts.switchRunlevel = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrPolicyDomain_t*, ocrRunlevel_t,
                                                    phase_t, u32, void (*)(ocrPolicyDomain_t*, u64), u64), handlelessCommSwitchRunlevel);
    base->apiFcts.initHandle = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrMsgHandle_t*), handlelessInitHandle);
    base->apiFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrLocation_t, ocrPolicyMsg_t *, ocrMsgHandle_t**, u32),
                                          handlelessCommSendMessage);
    base->apiFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrMsgHandle_t**),
                                          handlelessCommPollMessage);
    base->apiFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrMsgHandle_t**),
                                          handlelessCommWaitMessage);

    return base;
}
#endif /* ENABLE_COMM_API_HANDLELESS */
