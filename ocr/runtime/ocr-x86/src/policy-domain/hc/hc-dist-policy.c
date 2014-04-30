
/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_HC_DIST

#include "debug.h"
#include "ocr-errors.h"
#include "ocr-policy-domain.h"
#include "ocr-sysboot.h"
#include "utils/hashtable.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#include "policy-domain/hc/hc-dist-policy.h"

//DIST-TODO bad ! This is to use a worker's wid to map to a comm-api
#include "worker/hc/hc-worker.h"
//DIST-TODO hack to support edt templates
#include "task/hc/hc-task.h"

#define DEBUG_TYPE POLICY

#define RETRIEVE_LOCATION_FROM_GUID_MSG(pd, dstLoc) \
    ocrFatGuid_t fatGuid__ = PD_MSG_FIELD(guid); \
    u8 res = guidLocation(pd, fatGuid__, &dstLoc); \
    ASSERT(!res);

#define RETRIEVE_LOCATION_FROM_GUID(pd, dstLoc, guid__) \
    ocrFatGuid_t fatGuid__; \
    fatGuid__.guid = guid__; \
    fatGuid__.metaDataPtr = NULL; \
    u8 res = guidLocation(pd, fatGuid__, &dstLoc); \
    ASSERT(!res);

ocrGuid_t hcDistRtEdtRemoteSatisfy(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    // Need to send a message to the rank targeted by the localProxy
    //Need to downcast to proxyEvent, retrieve the original dest guid
    //TODO resolve dstGuid from a proxyEvent data-structure or just pass it as paramv ?
    ocrGuid_t dstGuid = (ocrGuid_t) paramv[0];
    // TODO is it always ok depv's guid is the guid the proxyEvent has been satisfied with ?
    ocrGuid_t value = depv[0].guid;
    ocrEventSatisfy(dstGuid, value);
    return NULL_GUID;
}

//DIST-TODO copy-pasted from hc-policy.c, this stuff need to be refactored
static u8 hcDistGrabPd(ocrPolicyDomainHc_t *rself) {
    START_PROFILE(pd_hc_GrabPd);
    u32 newState = rself->state;
    u32 oldState;
    if((newState & 0xF) == 1) {
        do {
            // Try to grab it
            oldState = newState;
            newState += 16; // Increment the user count by 1, skips the bottom 4 bits
            newState = hal_cmpswap32(&(rself->state), oldState, newState);
            if(newState == oldState) {
                RETURN_PROFILE(0);
            } else {
                if(newState & 0x2) {
                    // The PD is shutting down
                    RETURN_PROFILE(OCR_EAGAIN);
                }
                // Some other thread incremented the reader count so
                // we try again
                ASSERT((newState & 0xF) == 1); // Just to make sure
            }
        } while(true);
    } else {
        RETURN_PROFILE(OCR_EAGAIN);
    }
}

//DIST-TODO copy-pasted from hc-policy.c, this stuff need to be refactored
static void hcDistReleasePd(ocrPolicyDomainHc_t *rself) {
    START_PROFILE(pd_hc_ReleasePd);
    u32 oldState = 0;
    u32 newState = rself->state;
    do {
        ASSERT(newState > 16); // We must at least be a user
        oldState = newState;
        newState -= 16;
        newState = hal_cmpswap32(&(rself->state), oldState, newState);
    } while(newState != oldState);
    RETURN_PROFILE();
}

/*
 * Handle messages requiring remote communications, delegate locals to shared memory implementation.
 */
u8 hcDistProcessMessage(ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg, u8 isBlocking) {
    //DIST-TODO what's the meaning of nonBlocking here ?
    //DIST-TODO how to double check that: msg->srcLocation has been filled by getCurrentEnv(..., &msg) ?

    // Determine message's recipient and properties:
    // If destination is not set, check if it is a message with an affinity.
    //  If there's an affinity specified:
    //  - Determine an associated location
    //  - Set the msg destination to that location
    //  - Nullify the affinity guid
    // Else assume destination is the current location

    u8 ret = 0;
    ret = hcDistGrabPd((ocrPolicyDomainHc_t*)self);
    if(ret) {
        return ret;
    }

    //DIST-TODO: message's size should be set systematically by caller. Do it here as a failsafe for now
    msg->size = sizeof(ocrPolicyMsg_t);

    //DIST-TODO would help to have a NO_LOC default value
    ocrLocation_t curLoc = self->myLocation;
    u32 properties = 0;
    // for debugging purpose
    ocrWorkerHc_t * worker;
    getCurrentEnv(NULL, (ocrWorker_t **)&worker, NULL, NULL);
    int wid = worker->id;
    switch(msg->type & PD_MSG_TYPE_ONLY) {
        case PD_MSG_WORK_CREATE:
        {
    #define PD_MSG msg
    #define PD_TYPE PD_MSG_WORK_CREATE
        DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] Processing PD_MSG_WORK_CREATE msg: %p\n", (int) self->myLocation, wid, msg);
        ocrGuid_t affinity = PD_MSG_FIELD(affinity.guid);
        //DIST-TODO: this is so wrong
        if (affinity != NULL_GUID) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] Processing PD_MSG_WORK_CREATE: remote creation %p\n", (int) self->myLocation, wid, msg);
            msg->destLocation = (ocrLocation_t) affinity;
            //DIST-TODO Do not propagate affinity
            // PD_MSG_FIELD(affinity.guid) = NULL_GUID;
        } else {
            msg->destLocation = curLoc;
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] Processing PD_MSG_WORK_CREATE: local creation %p\n", (int) self->myLocation, wid, msg);
        }
    #undef PD_MSG
    #undef PD_TYPE

        if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] Processing PD_MSG_WORK_CREATE: remote invocation\n", (int) self->myLocation);
            // we're receiving a message
            //DIST-TODO: hack: unpack template information
        #define PD_MSG msg
        #define PD_TYPE PD_MSG_WORK_CREATE
                ocrGuid_t tempGuid = PD_MSG_FIELD(templateGuid.guid);
        #undef PD_MSG
        #undef PD_TYPE
            // Query the guid provider
            u64 val;
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], tempGuid, &val, NULL);
            if (((void*)val) == NULL) {
                DPRINTF(DEBUG_LVL_VVERB,"[%d] Processing PD_MSG_WORK_CREATE: Query remote EDT template\n", (int) self->myLocation);
                // GUID is unknown, request a copy of the metadata
                //DIST-TODO: assumes default message size is enough to contain the template metadata
                ocrPolicyMsg_t msgClone;
                getCurrentEnv(NULL, NULL, NULL, &msgClone);
                #define PD_MSG (&msgClone)
                #define PD_TYPE PD_MSG_GUID_METADATA_CLONE
                    msgClone.type = PD_MSG_GUID_METADATA_CLONE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
                    PD_MSG_FIELD(guid.guid) = tempGuid;
                    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
                    u8 returnCode = self->fcts.processMessage(self, &msgClone, true);
                    ASSERT(returnCode == 0);
                    // On return, Need some more post-processing to make a copy of the metadata
                    // and set the fatGuid's metadata ptr to point to the copy
                    ocrTaskTemplate_t * templ = (ocrTaskTemplate_t *) self->fcts.pdMalloc(self, sizeof(ocrTaskTemplateHc_t));
                    hal_memCopy(templ, &PD_MSG_FIELD(metaData), sizeof(ocrTaskTemplateHc_t), false);
                    self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], tempGuid, (u64) templ);
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] Template GUID registered for %ld\n",(int) self->myLocation, tempGuid);
                #undef PD_MSG
                #undef PD_TYPE
                DPRINTF(DEBUG_LVL_VVERB,"[%d] Processing PD_MSG_WORK_CREATE: Retrieved remote EDT template\n", (int) self->myLocation);
            }
        }
        break;
        }
        case PD_MSG_DB_CREATE:
        {
    #define PD_MSG msg
    #define PD_TYPE PD_MSG_DB_CREATE
        ocrGuid_t affinity = PD_MSG_FIELD(affinity.guid);
        PD_MSG_FIELD(affinity.guid) = NULL_GUID;
        //DIST-TODO: this is so wrong
        if (affinity != NULL_GUID) {
            msg->destLocation = (ocrLocation_t) affinity;
        } else {
            msg->destLocation = curLoc;
        }

    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
    // Need to determine the destination of the message based
    // on the operation and guids it involves.
        case PD_MSG_WORK_DESTROY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_WORK_DESTROY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DEP_SATISFY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DEP_SATISFY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
        DPRINTF(DEBUG_LVL_VVERB,"[%d] Satisfy destination is %d\n", (int) self->myLocation, (int) msg->destLocation);
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DEP_ADD:
        {
            // Need to determine which PD is the recipient of the add dependence
            //PERF: with some inside knowledge of how things are registered on
            //one another, we can avoid unnecessary message exchange.
            // if(srcKind == OCR_GUID_EVENT) {
            //     // Equivalent to a satisfy, delegate to destination
            //     RETRIEVE_LOCATION_FROM_GUID(self, msg->destLocation, dest)
            // } else if(srcKind == OCR_GUID_EVENT) {
            //     // process at event's PD
            //     RETRIEVE_LOCATION_FROM_GUID(self, msg->destLocation, source)
            // } else {
            //     // Some sanity check
            //     ASSERT(srcKind == OCR_GUID_EDT);
            // }
            //Do some pre-processing on the message
            //DIST-TODO: this might change the message, is that ok wrt to API doxygen ?
            // hcDistPreProcessAddDependence(self, msg);
            break;
        }
        case PD_MSG_EVT_DESTROY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_EVT_DESTROY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DB_DESTROY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DB_DESTROY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_EDTTEMP_DESTROY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_EDTTEMP_DESTROY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_GUID_INFO:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_GUID_INFO
        //TODO What's the meaning of guid info in distributed ?
        // TODO PD_MSG_FIELD(guid);
        msg->destLocation = curLoc;
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_GUID_METADATA_CLONE:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_GUID_METADATA_CLONE
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_GUID_DESTROY:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_GUID_DESTROY
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_MGT_SHUTDOWN:
        {
            if ((msg->srcLocation == curLoc) && (msg->destLocation == curLoc)) {
                ASSERT(msg->destLocation == curLoc);
                u32 i = 0;
                while(i < self->neighborCount) {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] loop shutdown neighbors[%d] is %d\n", (int) self->myLocation, i, (int) self->neighbors[i]);
                    ocrPolicyMsg_t msgShutdown;
                    getCurrentEnv(NULL, NULL, NULL, &msgShutdown);
                    msgShutdown.destLocation = self->neighbors[i];
                    DPRINTF(DEBUG_LVL_VERB,"[%d] Send shutdown msg to %d\n", (int) self->myLocation, (int) msgShutdown.destLocation);
                    #define PD_MSG (&msgShutdown)
                    #define PD_TYPE PD_MSG_MGT_SHUTDOWN
                        msgShutdown.type = PD_MSG_MGT_SHUTDOWN | PD_MSG_REQUEST;
                        u8 returnCode = self->fcts.processMessage(self, &msgShutdown, true);
                        ASSERT(returnCode == 0);
                    #undef PD_MSG
                    #undef PD_TYPE
                    i++;
                }
                //NOTE: We shutdown here otherwise we need to change the state
                //to spin on from 18 to 34 in hcPolicyDomainStop because the double
                //acquisition caused by calling the base impl.
                //Shutdown are one way messages in distributed. Avoids having to
                //send a response when the comms are shutting down.
                self->fcts.stop(self);
                msg->type &= ~PD_MSG_REQUEST;
                hcDistReleasePd((ocrPolicyDomainHc_t*)self);
                return 0;
            } else if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
                // Local processing
                // Same issues as above regarding the pd's state
                self->fcts.stop(self);
                msg->type &= ~PD_MSG_REQUEST;
                hcDistReleasePd((ocrPolicyDomainHc_t*)self);
                return 0;
            }
            // Fall-through to send to other PD or local processing.
         break;
        }
        // filter out local messages
        case PD_MSG_DEP_DYNADD:
        case PD_MSG_DB_ACQUIRE:
        case PD_MSG_DB_RELEASE:
        case PD_MSG_DB_FREE:
        case PD_MSG_MEM_OP:
        case PD_MSG_MEM_ALLOC:
        case PD_MSG_MEM_UNALLOC:
        case PD_MSG_WORK_EXECUTE:
        case PD_MSG_EDTTEMP_CREATE:
        case PD_MSG_EVT_CREATE:
        case PD_MSG_EVT_GET:
        case PD_MSG_GUID_CREATE:
        case PD_MSG_COMM_TAKE:
        case PD_MSG_COMM_GIVE:
        case PD_MSG_DEP_REGSIGNALER:
        case PD_MSG_DEP_REGWAITER:
        case PD_MSG_DEP_UNREGSIGNALER:
        case PD_MSG_DEP_UNREGWAITER:
        case PD_MSG_SAL_OP:
        case PD_MSG_SAL_PRINT:
        case PD_MSG_SAL_READ:
        case PD_MSG_SAL_WRITE:
        case PD_MSG_SAL_TERMINATE:
        case PD_MSG_MGT_OP: //DIST-TODO this is probably not always local
        case PD_MSG_MGT_FINISH:
        case PD_MSG_MGT_REGISTER:
        case PD_MSG_MGT_UNREGISTER:
        {
            msg->destLocation = curLoc;
            // fall-through and let local PD to process
            break;
        }
        default:
            // This is just a fail-safe to make sure the
            // PD impl accounts for all type of messages.
            ASSERT(false && "Unsupported message type");
    }
    //DIST-TODO not sure what to do with those.
    // ocrDbReleaseocrDbMalloc, ocrDbMallocOffset, ocrDbFree, ocrDbFreeOffset

    //DIST-TODO hack to avoid using mpi ranks directly
    ASSERT(msg->destLocation >= 64);

    //DIST-TODO Need to clearly define when the message dest is set
    // Delegate msg to another PD
    if(msg->destLocation != curLoc) {
        //NOTE: Some of the messages logically require a response, but the PD can
        // already know what to return or can generate a response on behalf
        // of another PD and let him know after the fact. In that case, the PD may
        // void the PD_MSG_REQ_RESPONSE msg's type and treat the call as a one-way

        if (msg->type & PD_MSG_REQ_RESPONSE) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] processing two-way\n", (int) self->myLocation);
            // Since it's a two-way, we'll be waiting for the response and set PERSIST.
            // NOTE: underlying comm-layer may or may not make a copy of msg.
            properties |= TWOWAY_MSG_PROP;
            properties |= PERSIST_MSG_PROP;
            ocrMsgHandle_t * handle = NULL;
            self->fcts.sendMessage(self, msg->destLocation, msg, &handle, properties);
            // Wait on the response handler for the communication to complete.
            DPRINTF(DEBUG_LVL_VVERB,"[%d] wait for reply\n", (int) self->myLocation);
            self->fcts.waitMessage(self, &handle);
            DPRINTF(DEBUG_LVL_VVERB,"[%d] received reply\n", (int) self->myLocation);
            //IMPL: sanity check: we're reusing the original msg buffer for both
            //request and reply, thus msg pointers should be identical.
            ASSERT(handle->response == msg);
            // Deallocate handle
            handle->destruct(handle);
        } else {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] processing one-way request\n", (int) self->myLocation);
            // one-way request, several options:
            // - For more asynchrony can make a copy of original msg in 'sendMessage',
            //   'waitMessage' then becomes a no-op.
            // - Otherwise, need to wait for the message to have been sent out so that
            //   we're sure the stack-allocated 'msg' in the caller doesn't disappear (IMPL).

            //LIMITATION: For one-way we cannot use PERSIST and thus must request
            // a copy to be done because current implementation doesn't support
            // "waiting" on a one-way.
            self->fcts.sendMessage(self, msg->destLocation, msg, NULL, 0);
        }

    } else {
        // Local PD handles the message. msg's destination is curLoc
        //NOTE: 'msg' may be coming from 'self' or from a remote PD. It can
        // either be a request (that may need a response) or a response.
        ocrPolicyDomainHcDist_t * pdSelfDist = (ocrPolicyDomainHcDist_t *) self;
        ret = pdSelfDist->baseProcessMessage(self, msg, isBlocking);

        // Here, 'msg' content has changed if a response was required

        // If msg's destination is not the current location anymore, it means we were
        // processing an incoming request from another PD. Send the response now.
        if (msg->destLocation != curLoc) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] send response after local processing of msg %p\n", (int) self->myLocation, wid, msg);
            ASSERT(msg->type & PD_MSG_RESPONSE);
            //IMPL: It also means the message buffer is managed by the runtime
            // (as opposed to be on the user call stack calling in its PD).
            // Hence, we post the response as a one-way, persistent and no handle.
            // The message will be deallocated on one-way call completion.
            self->fcts.sendMessage(self, msg->destLocation, msg, NULL, PERSIST_MSG_PROP);
        }
    }
    hcDistReleasePd((ocrPolicyDomainHc_t*)self);
    return ret;
}

u8 hcDistPdSendMessage(ocrPolicyDomain_t* self, ocrLocation_t target, ocrPolicyMsg_t *message,
                   ocrMsgHandle_t **handle, u32 properties) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    //DIST-TODO: The PD should not know about underlying worker.
    //Want a one-one mapping, either build it lazily or use indexing
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.sendMessage(self->commApis[id], target, message, handle, properties);

    return ret;
}

u8 hcDistPdPollMessage(ocrPolicyDomain_t *self, ocrMsgHandle_t **handle) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);

    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.pollMessage(self->commApis[id], handle);

    return ret;
}

u8 hcDistPdWaitMessage(ocrPolicyDomain_t *self,  ocrMsgHandle_t **handle) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);

    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.waitMessage(self->commApis[id], handle);

    return ret;
}

#define NB_BUCKETS_EVENT_PROXY_MAP 10

void hcDistPolicyDomainStart(ocrPolicyDomain_t * pd) {
    ocrPolicyDomainHcDist_t * distPd = (ocrPolicyDomainHcDist_t *) pd;
    // initialize parent
    distPd->baseStart(pd);
    ocrEdtTemplateCreate(&distPd->remoteSatisfyTemplateGuid, hcDistRtEdtRemoteSatisfy, 1/*paramc*/, 1/*depc*/);
}

void hcDistPolicyDomainStop(ocrPolicyDomain_t * pd) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    // In OCR-distributed we need to drain messages from the
    // communication sub-system. This is to make sure the
    // runtime doesn't abruptely stop while shutdown messages
    // are being issued.
    // DIST-TODO: standardize runlevels
    int runlevel = 3;
    while (runlevel > 0) {
        DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop begin at RL %d\n", (int) pd->myLocation, runlevel);
        u64 maxCount = pd->workerCount;
        u64 i;
        for(i = 0; i < maxCount; i++) {
            //DIST-TODO until runlevels get standardized, we only
            //make communication workers go through runlevels
            ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) pd->workers[i];
            if (hcWorker->hcType == HC_WORKER_COMM) {
                DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop comm-worker at RL %d\n", (int) pd->myLocation, runlevel);
                pd->workers[i]->fcts.stop(pd->workers[i]);
            }
        }
        //DIST-TODO: Same thing here we assume all the comm-api are runlevel compatible
        maxCount = pd->commApiCount;
        for(i = 0; i < maxCount; i++) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop comm-api at RL %d\n", (int) pd->myLocation, runlevel);
            pd->commApis[i]->fcts.stop(pd->commApis[i]);
        }
        DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop end at RL %d END\n", (int) pd->myLocation, runlevel);
        runlevel--;
    }

    // call the regular stop
    DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop calling base stop\n", (int) pd->myLocation);
    ocrPolicyDomainHcDist_t * distPd = (ocrPolicyDomainHcDist_t *) pd;
    distPd->baseStop(pd);
}

ocrPolicyDomain_t * newPolicyDomainHcDist(ocrPolicyDomainFactory_t * factory,
#ifdef OCR_ENABLE_STATISTICS
                                      ocrStats_t *statsObject,
#endif
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance) {

    ocrPolicyDomainHcDist_t * derived = (ocrPolicyDomainHcDist_t *) runtimeChunkAlloc(sizeof(ocrPolicyDomainHcDist_t), NULL);
    ocrPolicyDomain_t * base = (ocrPolicyDomain_t *) derived;

#ifdef OCR_ENABLE_STATISTICS
    factory->initialize(factory, statsObject, base, costFunction, perInstance);
#else
    factory->initialize(factory, base, costFunction, perInstance);
#endif
    return base;
}

void initializePolicyDomainHcDist(ocrPolicyDomainFactory_t * factory,
#ifdef OCR_ENABLE_STATISTICS
                                  ocrStats_t *statsObject,
#endif
                                  ocrPolicyDomain_t *self, ocrCost_t *costFunction, ocrParamList_t *perInstance) {
    ocrPolicyDomainFactoryHcDist_t * derivedFactory = (ocrPolicyDomainFactoryHcDist_t *) factory;
    // Initialize the base policy-domain
#ifdef OCR_ENABLE_STATISTICS
    derivedFactory->baseInitialize(factory, statsObject, self, costFunction, perInstance);
#else
    derivedFactory->baseInitialize(factory, self, costFunction, perInstance);
#endif
    ocrPolicyDomainHcDist_t * hcDistPd = (ocrPolicyDomainHcDist_t *) self;
    hcDistPd->baseProcessMessage = derivedFactory->baseProcessMessage;
    hcDistPd->baseStart = derivedFactory->baseStart;
    hcDistPd->baseStop = derivedFactory->baseStop;
    hcDistPd->remoteSatisfyTemplateGuid = NULL_GUID;
}

static void destructPolicyDomainFactoryHcDist(ocrPolicyDomainFactory_t * factory) {
    runtimeChunkFree((u64)factory, NULL);
}

ocrPolicyDomainFactory_t * newPolicyDomainFactoryHcDist(ocrParamList_t *perType) {
    ocrPolicyDomainFactory_t * baseFactory = newPolicyDomainFactoryHc(perType);
    ocrPolicyDomainFcts_t baseFcts = baseFactory->policyDomainFcts;

    ocrPolicyDomainFactoryHcDist_t* derived = (ocrPolicyDomainFactoryHcDist_t*) runtimeChunkAlloc(sizeof(ocrPolicyDomainFactoryHcDist_t), (void *)10);
    ocrPolicyDomainFactory_t* derivedBase = (ocrPolicyDomainFactory_t*) derived;
    // Set up factory function pointers and members
    derivedBase->instantiate = newPolicyDomainHcDist;
    derivedBase->initialize = initializePolicyDomainHcDist;
    derivedBase->destruct =  destructPolicyDomainFactoryHcDist;
    derivedBase->policyDomainFcts = baseFcts;
    derived->baseInitialize = baseFactory->initialize;
    derived->baseStart = baseFcts.start;
    derived->baseStop = baseFcts.stop;
    derived->baseProcessMessage = baseFcts.processMessage;

    // specialize some of the function pointers
    derivedBase->policyDomainFcts.start = FUNC_ADDR(void(*)(ocrPolicyDomain_t*), hcDistPolicyDomainStart);
    derivedBase->policyDomainFcts.stop = FUNC_ADDR(void(*)(ocrPolicyDomain_t*), hcDistPolicyDomainStop);
    derivedBase->policyDomainFcts.processMessage = FUNC_ADDR(u8(*)(ocrPolicyDomain_t*,ocrPolicyMsg_t*,u8), hcDistProcessMessage);
    derivedBase->policyDomainFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrLocation_t, ocrPolicyMsg_t *, ocrMsgHandle_t**, u32),
                                                   hcDistPdSendMessage);
    derivedBase->policyDomainFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), hcDistPdPollMessage);
    derivedBase->policyDomainFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrPolicyDomain_t*, ocrMsgHandle_t**), hcDistPdWaitMessage);

    baseFactory->destruct(baseFactory);
    return derivedBase;
}

#endif /* ENABLE_POLICY_DOMAIN_HC_DIST */
