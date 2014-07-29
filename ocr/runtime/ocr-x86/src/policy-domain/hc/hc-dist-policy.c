
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
#include "ocr-placer.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

#include "policy-domain/hc/hc-dist-policy.h"

//DIST-TODO sep-concern: bad ! This is to use a worker's wid to map to a comm-api
#include "worker/hc/hc-worker.h"
//DIST-TODO cloning: sep-concern: need to know end type to support edt templates cloning
#include "task/hc/hc-task.h"

#define DEBUG_TYPE POLICY

#define RETRIEVE_LOCATION_FROM_MSG(pd, fname, dstLoc) \
    ocrFatGuid_t fatGuid__ = PD_MSG_FIELD(fname); \
    u8 res = guidLocation(pd, fatGuid__, &dstLoc); \
    ASSERT(!res);

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

// Data-structure used to store foreign DB information
//DIST-TODO cloning: This is a poor's man meta-data cloning for datablocks
typedef struct {
    u64 size;
    void * volatile ptr;
    u32 flags;
} ProxyDb_t;

ocrGuid_t hcDistRtEdtRemoteSatisfy(u32 paramc, u64* paramv, u32 depc, ocrEdtDep_t depv[]) {
    //Need to send a message to the rank targeted by the localProxy
    //Need to downcast to proxyEvent, retrieve the original dest guid
    //TODO resolve dstGuid from a proxyEvent data-structure or just pass it as paramv ?
    ocrGuid_t dstGuid = (ocrGuid_t) paramv[0];
    // TODO is it always ok depv's guid is the guid the proxyEvent has been satisfied with ?
    ocrGuid_t value = depv[0].guid;
    ocrEventSatisfy(dstGuid, value);
    return NULL_GUID;
}

//DIST-TODO grad/release PD: copy-pasted from hc-policy.c, this stuff need to be refactored
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

//DIST-TODO grad/release PD: copy-pasted from hc-policy.c, this stuff need to be refactored
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

u8 resolveRemoteMetaData(ocrPolicyDomain_t * self, ocrFatGuid_t * fGuid, u64 metaDataSize) {
    ocrGuid_t remoteGuid = fGuid->guid;
    u64 val;
    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], remoteGuid, &val, NULL);
    if (val == 0) {
        DPRINTF(DEBUG_LVL_VVERB,"[%d] resolveRemoteMetaData: Query remote GUID metadata\n", (int) self->myLocation);
        // GUID is unknown, request a copy of the metadata
        ocrPolicyMsg_t msgClone;
        getCurrentEnv(NULL, NULL, NULL, &msgClone);
        #define PD_MSG (&msgClone)
        #define PD_TYPE PD_MSG_GUID_METADATA_CLONE
            msgClone.type = PD_MSG_GUID_METADATA_CLONE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
            PD_MSG_FIELD(guid.guid) = remoteGuid;
            PD_MSG_FIELD(guid.metaDataPtr) = NULL;
            u8 returnCode = self->fcts.processMessage(self, &msgClone, true);
            ASSERT(returnCode == 0);
            // On return, Need some more post-processing to make a copy of the metadata
            // and set the fatGuid's metadata ptr to point to the copy
            void * metaDataPtr = self->fcts.pdMalloc(self, metaDataSize);
            hal_memCopy(metaDataPtr, &PD_MSG_FIELD(metaData), metaDataSize, false);
            //DIST-TODO Potentially multiple concurrent registerGuid on the same template
            self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], remoteGuid, (u64) metaDataPtr);
            val = (u64) metaDataPtr;
            DPRINTF(DEBUG_LVL_VVERB,"[%d] GUID registered for %ld\n",(int) self->myLocation, remoteGuid);
        #undef PD_MSG
        #undef PD_TYPE
        DPRINTF(DEBUG_LVL_VVERB,"[%d] resolveRemoteMetaData: Retrieved remote EDT template\n", (int) self->myLocation);
    }
    fGuid->metaDataPtr = (void *) val;
    return 0;
}

void getTemplateParamcDepc(ocrPolicyDomain_t * self, ocrFatGuid_t * fatGuid, u32 * paramc, u32 * depc) {
    // Need to deguidify the edtTemplate to know how many elements we're really expecting
    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], fatGuid->guid,
                                        (u64*)&fatGuid->metaDataPtr, NULL);
    ocrTaskTemplate_t * edtTemplate = (ocrTaskTemplate_t *) fatGuid->metaDataPtr;
    *paramc = edtTemplate->paramc;
    *depc = edtTemplate->depc;
}

/*
 * flags : flags of the acquired DB.
 */
static void * acquireLocalDb(ocrPolicyDomain_t * pd, ocrGuid_t dbGuid) {
    ocrTask_t *curTask = NULL;
    ocrPolicyMsg_t msg;
    getCurrentEnv(NULL, NULL, &curTask, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_ACQUIRE
    msg.type = PD_MSG_DB_ACQUIRE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = dbGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(edt.guid) = curTask->guid;
    PD_MSG_FIELD(edt.metaDataPtr) = curTask;
    PD_MSG_FIELD(properties) = DB_PROP_RT_ACQUIRE; // Runtime acquire
    PD_MSG_FIELD(returnDetail) = 0;
    // This call may fail if the policy domain goes down
    // while we are starting to execute
    if(pd->fcts.processMessage(pd, &msg, true)) {
        return NULL;
    }
    return PD_MSG_FIELD(ptr);
#undef PD_MSG
#undef PD_TYPE
}

static void releaseLocalDb(ocrPolicyDomain_t * pd, ocrGuid_t dbGuid, u64 size) {
    ocrTask_t *curTask = NULL;
    ocrPolicyMsg_t msg;
    getCurrentEnv(NULL, NULL, &curTask, &msg);
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_DB_RELEASE
    msg.type = PD_MSG_DB_RELEASE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
    PD_MSG_FIELD(guid.guid) = dbGuid;
    PD_MSG_FIELD(guid.metaDataPtr) = NULL;
    PD_MSG_FIELD(edt.guid) = curTask->guid;
    PD_MSG_FIELD(edt.metaDataPtr) = curTask;
    PD_MSG_FIELD(size) = size;
    PD_MSG_FIELD(properties) = DB_PROP_RT_ACQUIRE; // Runtime release
    PD_MSG_FIELD(returnDetail) = 0;
    // Ignore failures at this point
    pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
}

static void * findMsgPayload(ocrPolicyMsg_t * msg, u32 * size) {
    u64 msgHeaderSize = sizeof(ocrPolicyMsg_t);
    if (size != NULL) {
        *size = (msg->size - msgHeaderSize);
    }
    return ((char*) msg) + msgHeaderSize;
}

/*
 * Handle messages requiring remote communications, delegate locals to shared memory implementation.
 */
u8 hcDistProcessMessage(ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg, u8 isBlocking) {
    //DIST-TODO semantic: what's the meaning of isBlocking in processMessage signature ?
    //DIST-TODO msg setup: how to double check that: msg->srcLocation has been filled by getCurrentEnv(..., &msg) ?

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
    // Pointer we keep around in case we create a copy original message
    // and need to get back to it
    ocrPolicyMsg_t * originalMsg = msg;

    //DIST-TODO msg setup: message's size should be set systematically by caller. Do it here as a failsafe for now
    msg->size = sizeof(ocrPolicyMsg_t);

    //DIST-TODO affinity: would help to have a NO_LOC default value
    //The current assumption is that a locally generated message will have
    //src and dest set to the 'current' location. If the message has an affinity
    //hint, it is then used to potentially decide on a different destination.
    ocrLocation_t curLoc = self->myLocation;
    u32 properties = 0;
    // for debugging purpose
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    int wid = ((ocrWorkerHc_t *)worker)->id;
    // end

    // Try to automatically place datablocks and edts
    // Only support naive PD-based placement for now.
    suggestLocationPlacement(self, curLoc, (ocrLocationPlacer_t *) self->placer, msg);

    switch(msg->type & PD_MSG_TYPE_ONLY) {
        case PD_MSG_WORK_CREATE:
        {
    #define PD_MSG msg
    #define PD_TYPE PD_MSG_WORK_CREATE
        DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_WORK_CREATE msg: %p, template 0x%x\n", (int) self->myLocation, wid, msg, PD_MSG_FIELD(templateGuid.guid));
        // First query the guid provider to determine if we know the edtTemplate.
        resolveRemoteMetaData(self, &PD_MSG_FIELD(templateGuid), sizeof(ocrTaskTemplateHc_t));
        // The placer may have altered msg->destLocation
        if (msg->destLocation != curLoc) {
            // Check for implementation limitations
            ocrTask_t *task = NULL;
            getCurrentEnv(NULL, NULL, &task, NULL);
            ASSERT((task->finishLatch == NULL_GUID) && "LIMITATION: distributed finish-EDT not supported");

            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_WORK_CREATE: remote creation at %d msg:%p, template 0x%x\n", (int) self->myLocation, wid, msg->destLocation, msg, PD_MSG_FIELD(templateGuid.guid));
            // When it's a remote creation we need to serialize paramv/depv arguments if any
            if ((PD_MSG_FIELD(paramv) != NULL) || (PD_MSG_FIELD(depv) != NULL)) {
                // Check if we need to involve the EDT template
                if (((PD_MSG_FIELD(paramv) != NULL) && (PD_MSG_FIELD(paramc) == EDT_PARAM_DEF)) ||
                    ((PD_MSG_FIELD(depv) != NULL) && (PD_MSG_FIELD(depc) == EDT_PARAM_DEF))) {
                    // Make sure we have a copy of it locally and retrieve paramc/depc values
                    getTemplateParamcDepc(self, &PD_MSG_FIELD(templateGuid), &PD_MSG_FIELD(paramc),&PD_MSG_FIELD(depc));
                }
                u64 extraParamvPayloadSize = ((PD_MSG_FIELD(paramv) != NULL) ? (sizeof(u64)*PD_MSG_FIELD(paramc)) : 0);
                u64 extraDepvPayloadSize = ((PD_MSG_FIELD(depv) != NULL) ? (sizeof(ocrFatGuid_t)*PD_MSG_FIELD(depc)) : 0);
                u64 extraPayloadSize = extraParamvPayloadSize + extraDepvPayloadSize;
                u64 msgWorkCreateSize = PD_MSG_SIZE_FULL(PD_MSG_WORK_CREATE);
                u64 newMsgSize = (msgWorkCreateSize + extraPayloadSize);
                // See if that fits in the default max message size.
                if (newMsgSize > sizeof(ocrPolicyMsg_t)) {
                    // slow path, we need to make a copy of the whole message
                    // Use the clonedMsg to 'remember' we need to deallocate this on communication completion.
                    ocrPolicyMsg_t * newMsg = self->fcts.pdMalloc(self, newMsgSize);
                    hal_memCopy(newMsg, msg, msgWorkCreateSize, false);
                    msg = newMsg;
                    msg->size = newMsgSize;
                }
                // Copy the extra payload
                if (extraParamvPayloadSize > 0) {
                    void * extraPayloadPtr = (((char*)msg) + msgWorkCreateSize);
                    hal_memCopy(extraPayloadPtr, PD_MSG_FIELD(paramv), extraParamvPayloadSize, false);
                }
                if (extraDepvPayloadSize > 0) {
                    void * extraPayloadPtr = (((char*)msg) + (msgWorkCreateSize + extraParamvPayloadSize));
                    hal_memCopy(extraPayloadPtr, PD_MSG_FIELD(depv), extraDepvPayloadSize, false);
                }
                //PD_MSG_FIELD(paramv) and PD_MSG_FIELD(depv) will point to junk on remote PD, which will
                //indicate paramv/depv were indeed set on the source location and that deserialization must
                //be performed. Using this (i.e. paramv/depv being not NULL) rather than paramc/depc avoids
                //deguidifying the edtTemplate for nothing when EDT_PARAM_DEF is used.
            }
        } else {
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_WORK_CREATE: local creation %p, template 0x%x\n", (int) self->myLocation, wid, msg, PD_MSG_FIELD(templateGuid.guid));
        }
    #undef PD_MSG
    #undef PD_TYPE

        if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
            // we're receiving a message
        #define PD_MSG msg
        #define PD_TYPE PD_MSG_WORK_CREATE
            // Second, check if we need to unpack paramv/depv
            if ((PD_MSG_FIELD(paramv) != NULL) || (PD_MSG_FIELD(depv) != NULL)) {
                // Check if we need to read information from the EDT template
                if (((PD_MSG_FIELD(paramv) != NULL) && (PD_MSG_FIELD(paramc) == EDT_PARAM_DEF)) ||
                    ((PD_MSG_FIELD(depv) != NULL) && (PD_MSG_FIELD(depc) == EDT_PARAM_DEF))) {
                    // Make sure we have a copy of it locally and retrieve paramc/depc values
                    getTemplateParamcDepc(self, &PD_MSG_FIELD(templateGuid), &PD_MSG_FIELD(paramc), &PD_MSG_FIELD(depc));
                }
                u64 msgWorkCreateSize = PD_MSG_SIZE_FULL(PD_MSG_WORK_CREATE);
                u64 extraParamvPayloadSize = (PD_MSG_FIELD(paramv) != NULL) ? (sizeof(u64)*PD_MSG_FIELD(paramc)) : 0;
                u64 extraDepvPayloadSize = (PD_MSG_FIELD(depv) != NULL) ? (sizeof(ocrFatGuid_t)*PD_MSG_FIELD(depc)) : 0;
                if (PD_MSG_FIELD(paramc) > 0) {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_WORK_CREATE: deserialize msg %p paramv, paramc=%d\n",
                                             (int) self->myLocation, wid, msg, PD_MSG_FIELD(paramc));
                    u64 extraPayloadSize = sizeof(u64)*PD_MSG_FIELD(paramc);
                    void * extraPayloadPtr = (void*) (((char*)msg) + msgWorkCreateSize);
                    u64 * paramvClone = (u64*) self->fcts.pdMalloc(self, extraParamvPayloadSize);
                    hal_memCopy(paramvClone, extraPayloadPtr, extraPayloadSize, false);
                    PD_MSG_FIELD(paramv) = paramvClone;
                }
                // depc alone is not enough to determine if depv where provided, we need to look
                // if src location had the message's depv pointing to something (which is junk here) but
                // still not NULL.
                if (PD_MSG_FIELD(depv) != NULL) {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_WORK_CREATE: deserialize msg %p depv, depc=%d\n",
                                             (int) self->myLocation, wid, msg, PD_MSG_FIELD(depc));
                    ocrGuid_t * extraPayloadPtr = (ocrGuid_t*) (((char*)msg) + (msgWorkCreateSize + extraParamvPayloadSize));
                    ocrFatGuid_t * depvClone = (ocrFatGuid_t*) self->fcts.pdMalloc(self, extraDepvPayloadSize);
                    int i =0;
                    while(i < PD_MSG_FIELD(depc)) {
                        depvClone[i].guid = extraPayloadPtr[i];
                        depvClone[i].metaDataPtr = NULL;
                        i++;
                    }
                    PD_MSG_FIELD(depv) = depvClone;
                }
            }
        }
        #undef PD_MSG
        #undef PD_TYPE
        break;
        }
        case PD_MSG_DB_CREATE:
        {
    #define PD_MSG msg
    #define PD_TYPE PD_MSG_DB_CREATE
        // The placer may have altered msg->destLocation
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
        case PD_MSG_DB_FREE:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DB_FREE
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
        //DIST-TODO cloning: What's the meaning of guid info in distributed ?
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
                //DIST-TODO These should be sent in parallel
                while(i < self->neighborCount) {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] loop shutdown neighbors[%d] is %d\n", (int) self->myLocation, i, (int) self->neighbors[i]);
                    ocrPolicyMsg_t msgShutdown;
                    getCurrentEnv(NULL, NULL, NULL, &msgShutdown);
                    msgShutdown.destLocation = self->neighbors[i];
                    DPRINTF(DEBUG_LVL_VERB,"[%d] Send shutdown msg to %d\n", (int) self->myLocation, (int) msgShutdown.destLocation);
                    // Shutdown is a two-way message. It gives the target the opportunity to drain and
                    // finalize some of its pending communication (think dbRelease called after the EDT
                    // that triggered shutdown on another node)
                    #define PD_MSG (&msgShutdown)
                    #define PD_TYPE PD_MSG_MGT_SHUTDOWN
                        msgShutdown.type = PD_MSG_MGT_SHUTDOWN | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
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
                msg->type |= PD_MSG_RESPONSE;
                hcDistReleasePd((ocrPolicyDomainHc_t*)self);
                return 0;
            } else if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
                // Local processing
                // Same issues as above regarding the pd's state
                u64 maxCount = self->workerCount;
                // When a sink EDT depends on a bunch of events. As soon as the events are
                // satisfied and sent out, the shutdown process may begin.
                // Here we wait for computation workers to wrap up the EDT they were executing.
                u64 i;
                for(i = 0; i < maxCount; i++) {
                    //DIST-TODO stop: see if that would go in a computation worker stop runlevel
                    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) self->workers[i];
                    if ((worker != ((ocrWorker_t *)hcWorker)) && (hcWorker->hcType == HC_WORKER_COMP)) {
                        DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST wait for comp-worker to wrap-up before shutdown %d\n", (int) self->myLocation);
                        while(self->workers[i]->curTask != NULL);
                    }
                }
                // Notify we're ready to shutdown
                // Important to send this message here and then call the PD's stop. The implementation
                // will make sure the outgoing messages are sent before terminating.
                ocrPolicyMsg_t shutdownReply;
                shutdownReply = *msg;
                shutdownReply.destLocation = msg->srcLocation;
                shutdownReply.srcLocation = curLoc;
                shutdownReply.type &= ~PD_MSG_REQUEST;
                shutdownReply.type |= PD_MSG_RESPONSE;
                self->fcts.sendMessage(self, shutdownReply.destLocation, &shutdownReply, NULL, 0);
                self->fcts.stop(self);
                hcDistReleasePd((ocrPolicyDomainHc_t*)self);
                return 0;
            }
            // Fall-through to send to other PD or local processing.
         break;
        }
        case PD_MSG_DEP_DYNADD:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DEP_DYNADD
            RETRIEVE_LOCATION_FROM_MSG(self, edt, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DEP_DYNREMOVE:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DEP_DYNREMOVE
            RETRIEVE_LOCATION_FROM_MSG(self, edt, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DB_ACQUIRE:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DB_ACQUIRE
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
        // Send/Receive to/from remote or local processing, all fall-through
        if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
            // Incoming acquire: Need to resolve the DB metadata locally before handing the message over
            u64 val;
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
            ASSERT(val != 0);
            PD_MSG_FIELD(guid.metaDataPtr) = (void *) val;
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_ACQUIRE: received request: DB guid 0x%lx properties=0x%lx\n",
                (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), PD_MSG_FIELD(properties));
        }
        if ((msg->srcLocation == curLoc) && (msg->destLocation != curLoc)) {
            // Outgoing acquire
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_ACQUIRE: remote request: DB guid 0x%lx msg %p properties=0x%lx\n",
                (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), msg, PD_MSG_FIELD(properties));
            // First check if we know about this foreign DB
            u64 val;
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
            if (val == 0) {
                // Don't know about this DB yet, request a fetch
                //There's a race here between first timers DB_ACQUIRE competing to fetch the DB
                //We would like to have a single worker doing the fetch to avoid wasting bandwidth
                //However coordinating the two can lead to deadlock in current implementation + blocking helper mode.
                //So, for now allow concurrent fetch and a single one will be allowed to commit to the registerGuid
                //DIST-TODO db: Revisit this locking scheme when we have a better idea of what we want to do with DB management
                DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_ACQUIRE: remote request: DB guid 0x%lx msg=%p requires FETCH\n", (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), msg);
                PD_MSG_FIELD(properties) |= DB_FLAG_RT_FETCH;
                // ocrPolicyDomainHcDist_t * typedSelf = (ocrPolicyDomainHcDist_t *) self;
                // hal_lock32(&(typedSelf->proxyLock));
                // // double check if someone went ahead of us
                // val = self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
                // if (val == 0) {
                //     DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_ACQUIRE: remote request: DB guid 0x%lx msg=%p requires FETCH\n", (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), msg);
                //     PD_MSG_FIELD(properties) |= DB_FLAG_RT_FETCH;
                //     // Store a local proxy for the DB
                //     ProxyDb_t * proxyDb = self->fcts.pdMalloc(self, sizeof(ProxyDb_t));
                //     proxyDb->size = 0;
                //     proxyDb->ptr = NULL;
                //     proxyDb->flags = 0;
                //     self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], PD_MSG_FIELD(guid.guid), (u64) proxyDb);
                // } // else nothing to do, just a regular acquire
                // hal_unlock32(&(typedSelf->proxyLock));
            }
        }
        if ((msg->srcLocation == curLoc) && (msg->destLocation == curLoc)) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_ACQUIRE: local request: DB guid 0x%lx properties 0x%lx\n",
                (int) self->myLocation, PD_MSG_FIELD(guid.guid), PD_MSG_FIELD(properties));
        }
        // Let the base policy's processMessage acquire the DB on behalf of the remote EDT
        // and then append the db data to the message.
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DB_RELEASE:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DB_RELEASE
        RETRIEVE_LOCATION_FROM_GUID_MSG(self, msg->destLocation)
        if ((msg->srcLocation != curLoc) && (msg->destLocation == curLoc)) {
            // Incoming DB_RELEASE pre-processing
            // Need to resolve the DB metadata locally before handing the message over
            u64 val;
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
            ASSERT(val != 0);
            PD_MSG_FIELD(guid.metaDataPtr) = (void *) val;
            DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_RELEASE received request: DB guid 0x%lx ptr, WB=%d\n",
                    (int) self->myLocation, PD_MSG_FIELD(guid.guid), !!(PD_MSG_FIELD(properties) & DB_FLAG_RT_WRITE_BACK));
            //DIST-TODO db: We may want to double check this writeback (first one) is legal wrt single assignment
            if (PD_MSG_FIELD(properties) & DB_FLAG_RT_WRITE_BACK) {
                // Unmarshall and write back
                DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_RELEASE received request: unmarshall DB guid 0x%lx ptr for local WB\n", (int) self->myLocation, PD_MSG_FIELD(guid.guid));
                //Important to read from the RELEASE size u64 field instead of the msg size (u32)
                u64 size = PD_MSG_FIELD(size);
                void * data = findMsgPayload(msg, NULL);
                // Acquire local DB
                void * localData = acquireLocalDb(self, PD_MSG_FIELD(guid.guid));
                hal_memCopy(localData, data, size, false);
                releaseLocalDb(self, PD_MSG_FIELD(guid.guid), size);
            } // else fall-through and do the regular release
        }
        if ((msg->srcLocation == curLoc) && (msg->destLocation != curLoc)) {
            // Outgoing DB_RELEASE pre-processing
            u64 val;
            self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
            if (val == 0) {
                //TODO db: early release only check the task extra-dbs, not dependences. Hence when a db obtained
                //from a dependence is released in the user code, the runtime will also try to release it at the end.
                //This is an issue for the distributed implementation because we've already deallocated the proxy db.
                //This need to be fixed in base implementation
                // For now just warn and ignore
                DPRINTF(DEBUG_LVL_WARN,"[%d][%d] PD_MSG_DB_RELEASE warning double release detected and avoided (hack) for DB guid 0x%lx ptr\n", (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid));
                msg->type &= ~PD_MSG_REQUEST;
                msg->type &= ~PD_MSG_REQ_RESPONSE;
                msg->type |= PD_MSG_RESPONSE;
                hcDistReleasePd((ocrPolicyDomainHc_t*)self);
                return 0;
            }
            ASSERT(val != 0);
            ProxyDb_t * proxyDb = (ProxyDb_t *) val;
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_RELEASE sending request: DB guid 0x%lx ptr properties=0x%x\n",
                (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), proxyDb->flags);
            if (proxyDb->flags & DB_FLAG_RT_WRITE_BACK) {
                PD_MSG_FIELD(properties) = DB_FLAG_RT_WRITE_BACK;
                if (proxyDb->flags & DB_PROP_SINGLE_ASSIGNMENT) {
                    // Clear the writeback flag. For single-assignment DB this prevent multiple illegal write-backs
                    proxyDb->flags &= ~DB_FLAG_RT_WRITE_BACK;
                }
                // Shipping the DB's data back to the home node
                DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_RELEASE sending request: marshall DB guid 0x%lx ptr for remote WB\n", (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid));
                u64 dbSize = proxyDb->size;
                void * dbPtr = proxyDb->ptr;
                // Prepare response message
                u64 msgHeaderSize = sizeof(ocrPolicyMsg_t);
                u64 msgSize = msgHeaderSize + dbSize;
                ocrPolicyMsg_t * newMessage = self->fcts.pdMalloc(self, msgSize); //DIST-TODO: should be able to use RELEASE real size + dbSize
                getCurrentEnv(NULL, NULL, NULL, newMessage);
                // Copy header
                hal_memCopy(newMessage, msg, msgHeaderSize, false);
                newMessage->size = msgSize;
                void * dbCopyPtr = ((char*)newMessage) + msgHeaderSize;
                //Copy DB's data in the message's payload
                hal_memCopy(dbCopyPtr, dbPtr, dbSize, false);
                msg = newMessage;
                PD_MSG_FIELD(size) = dbSize;
            }
        }
        if ((msg->srcLocation == curLoc) && (msg->destLocation == curLoc)) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] PD_MSG_DB_RELEASE local processing: DB guid 0x%lx\n", (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid));
        }
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DEP_REGSIGNALER:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DEP_REGSIGNALER
        RETRIEVE_LOCATION_FROM_MSG(self, dest, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        case PD_MSG_DEP_REGWAITER:
        {
    #define PD_MSG (msg)
    #define PD_TYPE PD_MSG_DEP_REGWAITER
        RETRIEVE_LOCATION_FROM_MSG(self, dest, msg->destLocation)
    #undef PD_MSG
    #undef PD_TYPE
        break;
        }
        // filter out local messages
        case PD_MSG_DEP_ADD:
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
        case PD_MSG_DEP_UNREGSIGNALER:
        //DIST-TODO not-supported: I don't think PD_MSG_DEP_UNREGSIGNALER is supported yet even in shared-memory
        case PD_MSG_DEP_UNREGWAITER:
        //DIST-TODO not-supported: I don't think PD_MSG_DEP_UNREGWAITER is support yet even in shared-memory
        case PD_MSG_SAL_OP:
        case PD_MSG_SAL_PRINT:
        case PD_MSG_SAL_READ:
        case PD_MSG_SAL_WRITE:
        case PD_MSG_SAL_TERMINATE:
        case PD_MSG_MGT_OP: //DIST-TODO not-supported: PD_MSG_MGT_OP is probably not always local
        case PD_MSG_MGT_FINISH:
        case PD_MSG_MGT_REGISTER:
        case PD_MSG_MGT_UNREGISTER:
        case PD_MSG_MGT_MONITOR_PROGRESS:
        {
            msg->destLocation = curLoc;
            // fall-through and let local PD to process
            break;
        }
        default:
            //DIST-TODO not-supported: not sure what to do with those.
            // ocrDbReleaseocrDbMalloc, ocrDbMallocOffset, ocrDbFree, ocrDbFreeOffset

            // This is just a fail-safe to make sure the
            // PD impl accounts for all type of messages.
            ASSERT(false && "Unsupported message type");
    }

    // By now, we must have decided what's the actual destination of the message

    // Delegate msg to another PD
    if(msg->destLocation != curLoc) {
        //NOTE: Some of the messages logically require a response, but the PD can
        // already know what to return or can generate a response on behalf
        // of another PD and let him know after the fact. In that case, the PD may
        // void the PD_MSG_REQ_RESPONSE msg's type and treat the call as a one-way

        // Message requires a response, send request and wait for response.
        if (msg->type & PD_MSG_REQ_RESPONSE) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d] processing two-way\n", (int) self->myLocation);
            // Since it's a two-way, we'll be waiting for the response and set PERSIST.
            // NOTE: underlying comm-layer may or may not make a copy of msg.
            properties |= TWOWAY_MSG_PROP;
            properties |= PERSIST_MSG_PROP;
            ocrMsgHandle_t * handle = NULL;

            self->fcts.sendMessage(self, msg->destLocation, msg, &handle, properties);
            // Wait on the response handle for the communication to complete.
            DPRINTF(DEBUG_LVL_VVERB,"[%d] wait for reply\n", (int) self->myLocation);
            self->fcts.waitMessage(self, &handle);
            DPRINTF(DEBUG_LVL_VVERB,"[%d] received reply\n", (int) self->myLocation);
            ASSERT(handle->response != NULL);

            // Check if we need to copy the response header over to the request msg.
            // Happens when the message includes some additional variable size payload
            // and request message cannot be reused. Or the underlying communication
            // platform was not able to reuse the request message buffer.

            //
            // Warning: From now on EXCLUSIVELY work on the response message
            //

            ocrPolicyMsg_t * response = handle->response;
            switch (response->type & PD_MSG_TYPE_ONLY) {
                case PD_MSG_DB_ACQUIRE:
                {
                // Receiving the reply for a DB_ACQUIRE
            #define PD_MSG (response)
            #define PD_TYPE PD_MSG_DB_ACQUIRE
                if (PD_MSG_FIELD(properties) & DB_FLAG_RT_FETCH) {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] Received DB_ACQUIRE FETCH reply ptr=%p size=%d properties=0x%x\n",
                        (int) self->myLocation, response, PD_MSG_FIELD(size), PD_MSG_FIELD(properties));
                    ocrPolicyDomainHcDist_t * typedSelf = (ocrPolicyDomainHcDist_t *) self;
                    // double check if someone went ahead of us
                    hal_lock32(&(typedSelf->proxyLock));
                    u64 val = self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
                    if (val == 0) {
                        // The acquire doubles as a fetch, need to deserialize DB's data from the message
                        PD_MSG_FIELD(properties) &= ~DB_FLAG_RT_FETCH;
                        // Set the pointer to the message payload
                        void * ptr = findMsgPayload(response, NULL);
                        u64 size = PD_MSG_FIELD(size);
                        // Copy data ptr from message to new buffer
                        void * newPtr = self->fcts.pdMalloc(self, size);
                        hal_memCopy(newPtr, ptr, size, false);
                        PD_MSG_FIELD(ptr) = newPtr;
                        // If the acquired DB is neither single assignment nor read-only
                        // we'll need to write it back on release
                        bool doWriteBack = !((PD_MSG_FIELD(properties) & DB_PROP_SINGLE_ASSIGNMENT) || (PD_MSG_FIELD(properties) & DB_MODE_RO));
                        if (doWriteBack) {
                            PD_MSG_FIELD(properties) |= DB_FLAG_RT_WRITE_BACK;
                        }
                        // Set up the dbProxy
                        ProxyDb_t * dbProxy = self->fcts.pdMalloc(self, sizeof(ProxyDb_t));
                        dbProxy->size = PD_MSG_FIELD(size);
                        dbProxy->flags = PD_MSG_FIELD(properties);
                        dbProxy->ptr = PD_MSG_FIELD(ptr);
                        self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], PD_MSG_FIELD(guid.guid), (u64) dbProxy);
                        DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] Commit DB_ACQUIRE FETCH db proxy setup guid=0x%lx ptr=%p size=%d properties=0x%x\n",
                            (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), dbProxy->ptr, dbProxy->size, dbProxy->flags);
                    } else {
                        ProxyDb_t * dbProxy = (ProxyDb_t*) val;
                        ASSERT(dbProxy->ptr != NULL);
                        DPRINTF(DEBUG_LVL_VVERB,"[%d] Discard Commit DB_ACQUIRE FETCH reply ptr=%p size=%d properties=0x%x\n",
                            (int) self->myLocation, dbProxy->ptr, dbProxy->size, dbProxy->flags);
                        PD_MSG_FIELD(size) = dbProxy->size;
                        PD_MSG_FIELD(ptr) = dbProxy->ptr;
                    }
                    hal_unlock32(&(typedSelf->proxyLock));
                } else {
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] Received DB_ACQUIRE reply\n", (int) self->myLocation);
                    // Acquire without fetch (Runtime detected data was already available locally and usable).
                    // Hence the message size should be the regular policy message size
                    ASSERT(msg->size == sizeof(ocrPolicyMsg_t));
                    // Here we're potentially racing with an acquire populating the db proxy
                    u64 val;
                    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], PD_MSG_FIELD(guid.guid), &val, NULL);
                    ASSERT(val != 0); // The registration should ensure there's a DB proxy set up
                    ProxyDb_t * dbProxy = (ProxyDb_t*) val;
                    //DIST-TODO db: this is borderline. Since the other worker is populating several fields,
                    //when 'ptr' is written it is not guaranteed the other assignment are visible yet.
                    DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] Waiting for DB_ACQUIRE reply guid=0x%lx ptr=%p size=%d properties=0x%x \n",
                        (int) self->myLocation, wid, PD_MSG_FIELD(guid.guid), dbProxy->ptr, dbProxy->size, dbProxy->flags);
                    ASSERT(dbProxy->ptr != NULL);
                    // while(dbProxy->ptr == NULL); // busy-wait for data to become available
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] Received DB_ACQUIRE reply ptr=%p size=%d properties=0x%x\n",
                        (int) self->myLocation, dbProxy->ptr, dbProxy->size, dbProxy->flags);
                    PD_MSG_FIELD(size) = dbProxy->size;
                    PD_MSG_FIELD(ptr) = dbProxy->ptr;
                }
            #undef PD_MSG
            #undef PD_TYPE
                break;
                }
                case PD_MSG_DB_CREATE:
                {
                    // Receiving the reply for a DB_CREATE
                    // Create a db proxy
                #define PD_MSG (response)
                #define PD_TYPE PD_MSG_DB_CREATE
                    ocrGuid_t dbGuid = PD_MSG_FIELD(guid.guid);
                    u64 size = PD_MSG_FIELD(size);
                    // Create the local ptr
                    void * ptr = self->fcts.pdMalloc(self, size);
                    // Double check there's no previous registration
                    u64 val;
                    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], dbGuid, &val, NULL);
                    ASSERT(val == 0);
                    // Register the proxy DB
                    ProxyDb_t * proxyDb = self->fcts.pdMalloc(self, sizeof(ProxyDb_t));
                    proxyDb->size = size;
                    proxyDb->ptr = ptr;
                    // Preset the writeback flag: even single assignment needs to be written back the first time.
                    proxyDb->flags = PD_MSG_FIELD(properties) | DB_FLAG_RT_WRITE_BACK;
                    self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], dbGuid, (u64) proxyDb);
                    PD_MSG_FIELD(ptr) = ptr;
                #undef PD_MSG
                #undef PD_TYPE
                break;
                }
                case PD_MSG_DB_RELEASE:
                {
                #define PD_MSG (response)
                #define PD_TYPE PD_MSG_DB_RELEASE
                    DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_RELEASE received response for DB guid 0x%lx\n",
                        (int) self->myLocation, PD_MSG_FIELD(guid.guid));
                    ocrGuid_t dbGuid = PD_MSG_FIELD(guid.guid);
                    u64 val;
                    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], dbGuid, &val, NULL);
                    ASSERT(val != 0);
                    ProxyDb_t * proxyDb = (ProxyDb_t *) val;
                    // Current model keeps SA DBs around, otherwise the proxy is nullified
                    // so that we're sure we always fetch an updated DB
                    if (!(proxyDb->flags & DB_PROP_SINGLE_ASSIGNMENT)) {
                        DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_RELEASE received response: cleanup local proxy for DB guid 0x%lx, ptr=%p size=%d properties=0x%x\n",
                            (int) self->myLocation, PD_MSG_FIELD(guid.guid), proxyDb->ptr, proxyDb->size, proxyDb->flags);
                        self->fcts.pdFree(self, proxyDb->ptr);
                        self->fcts.pdFree(self, proxyDb);
                        // cleanup guid provider entry to detect racy acquire
                        self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], dbGuid, (u64) 0);
                    }
                #undef PD_MSG
                #undef PD_TYPE
                break;
                }
                default: {
                    break;
                }
            }

            //
            // At this point the response message is ready to be returned
            //

            //Since the caller only has access to the original message we need
            //to make sure it's up-to-date

            if (originalMsg != msg) {
                // There's been a request message copy (maybe to accomodate larger outgoing message payload)
                hal_memCopy(originalMsg, handle->response, sizeof(ocrPolicyMsg_t), true);
                // Check if the request message has also been used for the response
                if (msg != handle->response) {
                    self->fcts.pdFree(self, msg);
                }
                self->fcts.pdFree(self, handle->response);
            } else {
                if (msg != handle->response) {
                    // Response is in a different message, copy and free
                    hal_memCopy(originalMsg, handle->response, sizeof(ocrPolicyMsg_t), true);
                    self->fcts.pdFree(self, handle->response);
                }
            }
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

        // Here, 'msg' content has potentially changed if a response was required

        // If msg's destination is not the current location anymore, it means we were
        // processing an incoming request from another PD. Send the response now.
        if (msg->destLocation != curLoc) {
            DPRINTF(DEBUG_LVL_VVERB,"[%d][%d] send response after local processing of msg %p\n", (int) self->myLocation, wid, msg);
            ASSERT(msg->type & PD_MSG_RESPONSE);
            switch(msg->type & PD_MSG_TYPE_ONLY) {
            case PD_MSG_DB_ACQUIRE:
            {
            #define PD_MSG (msg)
            #define PD_TYPE PD_MSG_DB_ACQUIRE
                DPRINTF(DEBUG_LVL_VVERB,"[%d] PD_MSG_DB_ACQUIRE: post-process response, guid=0x%lx serialize DB's ptr, dest is [%d]\n",
                    (int) curLoc, PD_MSG_FIELD(guid.guid), (int) msg->destLocation);
                // The base policy acquired the DB on behalf of the remote EDT
                if (PD_MSG_FIELD(properties) & DB_FLAG_RT_FETCH) {
                    // fetch was requested, serialize the DB data into response
                    u64 dbSize = PD_MSG_FIELD(size);
                    void * dbPtr = PD_MSG_FIELD(ptr);
                    // Prepare response message
                    u64 msgHeaderSize = sizeof(ocrPolicyMsg_t);
                    u64 msgSize = msgHeaderSize + dbSize;
                    ocrPolicyMsg_t * responseMessage = self->fcts.pdMalloc(self, msgSize); //DIST-TODO: should be able to use ACQUIRE real size + dbSize
                    getCurrentEnv(NULL, NULL, NULL, responseMessage);
                    hal_memCopy(responseMessage, msg, msgHeaderSize, false);
                    responseMessage->size = msgSize;
                    void * dbCopyPtr = findMsgPayload(responseMessage, NULL);
                    //Copy DB's data in the message's payload
                    hal_memCopy(dbCopyPtr, dbPtr, dbSize, false);
                    //NOTE: The DB will be released on DB_RELEASE
                    // Since this is a response to a remote PD request, the request
                    // message buffer is runtime managed so we need to deallocate it.
                    self->fcts.pdFree(self, msg);
                    msg = responseMessage;
                }
            #undef PD_MSG
            #undef PD_TYPE
            break;
            }
            default: { }
            }
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
    //DIST-TODO sep-concern: The PD should not know about underlying worker impl
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.sendMessage(self->commApis[id], target, message, handle, properties);

    return ret;
}

u8 hcDistPdPollMessage(ocrPolicyDomain_t *self, ocrMsgHandle_t **handle) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    //DIST-TODO sep-concern: The PD should not know about underlying worker impl
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.pollMessage(self->commApis[id], handle);

    return ret;
}

u8 hcDistPdWaitMessage(ocrPolicyDomain_t *self,  ocrMsgHandle_t **handle) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    //DIST-TODO sep-concern: The PD should not know about underlying worker impl
    ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) worker;
    int id = hcWorker->id;
    u8 ret = self->commApis[id]->fcts.waitMessage(self->commApis[id], handle);

    return ret;
}

void hcDistPolicyDomainStart(ocrPolicyDomain_t * pd) {
    ocrPolicyDomainHcDist_t * distPd = (ocrPolicyDomainHcDist_t *) pd;
    // initialize parent
    distPd->baseStart(pd);
}

void hcDistPolicyDomainStop(ocrPolicyDomain_t * pd) {
    ocrWorker_t * worker;
    getCurrentEnv(NULL, &worker, NULL, NULL);
    // In OCR-distributed we need to drain messages from the
    // communication sub-system. This is to make sure the
    // runtime doesn't abruptely stop while shutdown messages
    // are being issued.
    // DIST-TODO stop: standardize runlevels
    int runlevel = 3;
    u64 i = 0;
    while (runlevel > 0) {
        DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop begin at RL %d\n", (int) pd->myLocation, runlevel);
        u64 maxCount = pd->workerCount;
        for(i = 0; i < maxCount; i++) {
            //DIST-TODO stop: until runlevels get standardized, we only
            //make communication workers go through runlevels
            ocrWorkerHc_t * hcWorker = (ocrWorkerHc_t *) pd->workers[i];
            if (hcWorker->hcType == HC_WORKER_COMM) {
                DPRINTF(DEBUG_LVL_VVERB,"[%d] HC-DIST stop comm-worker at RL %d\n", (int) pd->myLocation, runlevel);
                pd->workers[i]->fcts.stop(pd->workers[i]);
            }
        }
        //DIST-TODO stop: Same thing here we assume all the comm-api are runlevel compatible
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
