/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_API_HC

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "ocr-worker.h"
#include "ocr-comm-platform.h"
#include "utils/ocr-utils.h"
#include "comm-api/hc/hc-comm-api.h"

#define DEBUG_TYPE COMM_API

// HC_COMM masks
#define HC_COMM_NO_PROP 0
#define HC_COMM_NO_MASK 0

//PERF: customize map size
#define HANDLE_MAP_BUCKETS 10

static void destructMsgHandler(ocrMsgHandle_t * handler) {
    ocrPolicyDomain_t *pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    pd->fcts.pdFree(pd, handler);
}

/**
 * @brief Create a message handler for this comm-api
 */
static ocrMsgHandle_t * createMsgHandler(ocrCommApi_t * self, ocrPolicyMsg_t * message) {
    ocrPolicyDomain_t * pd = self->pd;
    ocrMsgHandle_t * handler = pd->fcts.pdMalloc(pd, sizeof(ocrMsgHandle_t));
    handler->msg = message;
    handler->response = NULL;
    handler->status = HDL_NORMAL;
    handler->destruct = &destructMsgHandler;
    return handler;
}

static ocrPolicyMsg_t * allocateNewMessage(ocrCommApi_t * self, u32 size) {
    ocrPolicyDomain_t * pd = self->pd;
    ocrPolicyMsg_t * message = pd->fcts.pdMalloc(pd, size);
    return message;
}

u8 sendMessageHcCommApi(ocrCommApi_t *self, ocrLocation_t target, ocrPolicyMsg_t *message,
                        ocrMsgHandle_t **handle, u32 properties) {
    ocrCommApiHc_t * commApiHc = (ocrCommApiHc_t *) self;

    if (!(properties & PERSIST_MSG_PROP)) {
        ocrPolicyMsg_t * msgCpy = allocateNewMessage(self, message->size);
        hal_memCopy(msgCpy, message, message->size, false);
        message = msgCpy;
        properties &= PERSIST_MSG_PROP;
    }

    u64 bufferSize = message->size;
    u64 id = 0;

    u8 ret = self->commPlatform->fcts.sendMessage(self->commPlatform, target, message,
                                                  bufferSize, &id, properties, HC_COMM_NO_MASK);

    if (ret == 0) {
        if (handle == NULL) {
            // No handle requested
        } else {
            if (*handle == NULL) {
                // Handle creation requested
                *handle = createMsgHandler(self, message);
            }
            //DIST-TODO: is it ok to consider message sent here
            (*handle)->status = HDL_SEND_OK;
            // Associate id with handle
            hashtableNonConcPut(commApiHc->handleMap, (void *) id, *handle);
        }
    } else {
        // Error occurred while sending, set handler status if any
        if ((handle != NULL) && (*handle != NULL)) {
            (*handle)->status = HDL_SEND_ERR;
        }
    }

    return ret;
}

/*
 * Callers should look at the handle's status.
 *
 */
u8 pollMessageHcCommApi(ocrCommApi_t *self, ocrMsgHandle_t **handle) {

    //DIST-TODO: This implementation does not support one-way with ack
    //However it's a little hard to detect here, because a handle's message
    //may not be valid and it's the only place where we can.

    //IMPL: Alternative would be that the comm-platform always return
    //out/in comms that are done and we can update the status here.
    //That would require to be able to tell the platform to post new recv.
    ocrCommApiHc_t * commApiHc = (ocrCommApiHc_t *) self;
    ocrPolicyMsg_t * msg = NULL;
    //DIST-TODO: a contract here is probably wrong because it makes assumptions
    //IMPL: by contract commPlatform only poll and return recvs.
    //They can be incoming request or response. (but not outgoing req/resp ack)
    u8 ret = self->commPlatform->fcts.pollMessage(self->commPlatform, &msg, NULL, HC_COMM_NO_PROP, HC_COMM_NO_MASK);
    if (ret == POLL_MORE_MESSAGE) {
        ASSERT((handle != NULL) && (*handle == NULL));
        if (msg->type & PD_MSG_REQUEST) {
            // This an outstanding request, we need to create a handle for
            // the caller to manipulate the message.
            *handle = createMsgHandler(self, msg);
        } else {
            // Response for a request
            //NOTE: If the outgoing communication requires a response, the communication layer
            //      must set the handler->response pointer to the response's communication handler.

            ASSERT(msg->type & PD_MSG_RESPONSE);
            bool found = hashtableNonConcRemove(commApiHc->handleMap, (void *) msg->msgId, (void **)handle);
            ASSERT(found && (*handle != NULL));
            (*handle)->response = msg;
            (*handle)->status = HDL_RESPONSE_OK;
        }
    }
    return ret;
}

u8 waitMessageHcCommApi(ocrCommApi_t *self, ocrMsgHandle_t **handle) {
    u8 ret = 0;
    do {
        ret = self->fcts.pollMessage(self, handle);
    } while(ret != POLL_MORE_MESSAGE);

    return ret;
}

void hcCommApiDestruct (ocrCommApi_t * base) {
    base->commPlatform->fcts.destruct(base->commPlatform);
    runtimeChunkFree((u64)base, NULL);
}

void hcCommApiBegin(ocrCommApi_t* self, ocrPolicyDomain_t * pd) {
    self->pd = pd;
    self->commPlatform->fcts.begin(self->commPlatform, pd, self);
}

void hcCommApiStart(ocrCommApi_t * self, ocrPolicyDomain_t * pd) {
    ocrCommApiHc_t * commApiHc = (ocrCommApiHc_t *) self;
    commApiHc->handleMap = newHashtableModulo(pd, HANDLE_MAP_BUCKETS);
    self->commPlatform->fcts.start(self->commPlatform, pd, self);
}

void hcCommApiStop(ocrCommApi_t * self) {
    self->commPlatform->fcts.stop(self->commPlatform);
}

void hcCommApiFinish(ocrCommApi_t *self) {
    self->commPlatform->fcts.finish(self->commPlatform);
}

ocrCommApi_t* newCommApiHc(ocrCommApiFactory_t *factory,
                           ocrParamList_t *perInstance) {
    ocrCommApiHc_t * commApiHc = (ocrCommApiHc_t*)
        runtimeChunkAlloc(sizeof(ocrCommApiHc_t), NULL);
    factory->initialize(factory, (ocrCommApi_t*) commApiHc, perInstance);
    return (ocrCommApi_t*)commApiHc;
}

/**
 * @brief Initialize an instance of comm-api hc
 */
void initializeCommApiHc(ocrCommApiFactory_t * factory, ocrCommApi_t* self, ocrParamList_t * perInstance) {
    initializeCommApiOcr(factory, self, perInstance);
    ocrCommApiHc_t * commApiHc = (ocrCommApiHc_t*) self;
    commApiHc->handleMap = NULL;
}

/******************************************************/
/* OCR COMM API HC                                    */
/******************************************************/

void destructCommApiFactoryHc(ocrCommApiFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrCommApiFactory_t *newCommApiFactoryHc(ocrParamList_t *perType) {
    ocrCommApiFactory_t *base = (ocrCommApiFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCommApiFactoryHc_t), (void *)1);

    base->instantiate = newCommApiHc;
    base->initialize = initializeCommApiHc;
    base->destruct = destructCommApiFactoryHc;

    base->apiFcts.destruct = FUNC_ADDR(void (*)(ocrCommApi_t*), hcCommApiDestruct);
    base->apiFcts.begin = FUNC_ADDR(void (*)(ocrCommApi_t*, ocrPolicyDomain_t*),
                                    hcCommApiBegin);
    base->apiFcts.start = FUNC_ADDR(void (*)(ocrCommApi_t*, ocrPolicyDomain_t*),
                                    hcCommApiStart);
    base->apiFcts.stop = FUNC_ADDR(void (*)(ocrCommApi_t*), hcCommApiStop);
    base->apiFcts.finish = FUNC_ADDR(void (*)(ocrCommApi_t*), hcCommApiFinish);
    base->apiFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrLocation_t, ocrPolicyMsg_t *, ocrMsgHandle_t**, u32),
                                          sendMessageHcCommApi);
    base->apiFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrMsgHandle_t**),
                                          pollMessageHcCommApi);
    base->apiFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommApi_t*, ocrMsgHandle_t**),
                                          waitMessageHcCommApi);
    return base;
}

#endif /* ENABLE_COMM_API_HC */

