/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"
#ifdef ENABLE_COMM_PLATFORM_MPI

#include "debug.h"

#include "ocr-sysboot.h"
#include "ocr-policy-domain.h"
#include "ocr-worker.h"

#include "utils/ocr-utils.h"

#include "mpi-comm-platform.h"
#include <mpi.h>

#define DEBUG_TYPE COMM_PLATFORM


/*
- Posts recv any at start and whevener a request is received to compensate
- When a recv any is completed its message id will be 0
- Always use the handle->msg to store the reply (made a copy if necessary)
*/


// We apply the rank shift to the MPI rank and use
// that as the PD's location so that it looks more
// like a location and not a rank.
#define HACK_RANK_SHIFT 64

//
// MPI library Init/Finalize
//

/**
 * @brief Initialize the MPI library.
 */
void platformInitMPIComm(int argc, char ** argv) {
    int res = MPI_Init(&argc, &argv);
    ASSERT(res == MPI_SUCCESS);
}

/**
 * @brief Finalize the MPI library (no more remote calls after that).
 */
void platformFinalizeMPIComm() {
    int res = MPI_Finalize();
    ASSERT(res == MPI_SUCCESS);
}

//DIST-HACK7: Blessed worker is from MPI rank '0'
bool mpiBlessed() {
    int rank=0;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    return rank == 0;
}

static int locationToMPIRank(ocrLocation_t target) {
    //DIST-HACK5: what is a location, where do we keep location/rank assoc ?
    return ((int) target)-HACK_RANK_SHIFT;
}


// Pre-post recvs before sends using the message's id as a tag
#define PRE_POST_RECV 1
#define RECV_ANY_ID 0
#define SEND_ANY_ID 0

typedef struct {
    u64 msgId; // The MPI comm layer message id for this communication
    u32 properties;
    ocrPolicyMsg_t * msg;
    MPI_Request status;
} mpiCommHandle_t;


/**
 * @brief Internal use - Returns a new message
 */
static ocrPolicyMsg_t * allocateNewMessage(ocrCommPlatform_t * self, u32 size) {
    ocrPolicyDomain_t * pd = self->pd;
    ocrPolicyMsg_t * message = pd->fcts.pdMalloc(pd, size);
    return message;
}

/**
 * @brief Internal use - Create a mpi handle to represent pending communications
 */
static mpiCommHandle_t * createMpiHandle(ocrCommPlatform_t * self, u64 id, u32 properties, ocrPolicyMsg_t * msg) {
    ocrPolicyDomain_t * pd = self->pd;
    mpiCommHandle_t * handle = pd->fcts.pdMalloc(pd, sizeof(mpiCommHandle_t));
    handle->msgId = id;
    handle->properties = properties;
    handle->msg = msg;
    return handle;
}

/**
 * @brief Internal use - Asks the comm-platform to listen for incoming communication.
 */
static void postRecvAny(ocrCommPlatform_t * self) {
    ocrCommPlatformMPI_t * mpiComm = (ocrCommPlatformMPI_t *) self;
    ocrPolicyMsg_t * msg = allocateNewMessage(self, mpiComm->maxMsgSize);
    mpiCommHandle_t * handle = createMpiHandle(self, RECV_ANY_ID, PERSIST_MSG_PROP, msg);
    void * buf = msg; // Reuse request message as receive buffer
    int count = mpiComm->maxMsgSize; // don't know what to expect, upper-bound on message size
    MPI_Datatype datatype = MPI_BYTE;
    int src = MPI_ANY_SOURCE;
    int tag = RECV_ANY_ID;
    MPI_Comm comm = MPI_COMM_WORLD;
    DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] posting irecv ANY \n", (int) self->pd->myLocation);
    int res = MPI_Irecv(buf, count, datatype, src, tag, comm, &(handle->status));
    ASSERT(res == MPI_SUCCESS);
    mpiComm->incoming->pushFront(mpiComm->incoming, handle);
}

//
// Communication API
//
u8 MPICommSendMessage(ocrCommPlatform_t * self,
                      ocrLocation_t target, ocrPolicyMsg_t * message,
                      u64 bufferSize, u64 *id, u32 properties, u32 mask) {

    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);

    if (!(properties & PERSIST_MSG_PROP)) {
        ocrPolicyMsg_t * msgCpy = allocateNewMessage(self, message->size);
        hal_memCopy(msgCpy, message, message->size, false);
        message = msgCpy;
        properties &= PERSIST_MSG_PROP;
    }

    //DIST-TODO: msgId incr only works if a single comm-worker per rank,
    //do we want OCR to provide PD, system level counters ?
    //Generate an identifier for this communication
    u64 mpiId = mpiComm->msgId++;

    // If we're sending a request, set the message's msgId to this communication id
    if (message->type & PD_MSG_REQUEST) {
        message->msgId = mpiId;
    }
    // else, for responses, just keep the original
    // message's msgId the calling PD is waiting on.

    // Prepare MPI call arguments
    MPI_Datatype datatype = MPI_BYTE;
    int targetRank = locationToMPIRank(target);
    ASSERT(targetRank > -1);
    MPI_Comm comm = MPI_COMM_WORLD;

    #ifdef PRE_POST_RECV
    // Setup request's response with an MPI recv
    if (message->type & PD_MSG_REQ_RESPONSE) {
        // Reuse request message as receive buffer unless indicated otherwise
        ocrPolicyMsg_t * respMsg = message;
        //DIST-TODO: here we could do better if the response for this message's type is of fixed-length.
        int respCount = mpiComm->maxMsgSize;
        int respTag = mpiId;
        // Prepare a handle for the incoming response
        mpiCommHandle_t * respHandle = createMpiHandle(self, mpiId, properties, respMsg);
        MPI_Request * status = &(respHandle->status);
        //Post a receive matching the request's msgId.
        //The other end will post a send using msgId as tag
        DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] posting irecv for msgId %ld\n", (int) self->pd->myLocation, respTag);
        int res = MPI_Irecv(respMsg, respCount, datatype, targetRank, respTag, comm, status);
        if (res != MPI_SUCCESS) {
            //DIST-TODO define error for comm-api
            ASSERT(false);
            return res;
        }
        mpiComm->incoming->pushFront(mpiComm->incoming, respHandle);
    }
    #endif
    // Setup request's MPI send
    mpiCommHandle_t * handle = createMpiHandle(self, mpiId, properties, message);
    // If this send is for a response, use message's msgId as tag to
    // match the source recv operation that had been posted on the request send.
    int tag = (message->type & PD_MSG_RESPONSE) ? message->msgId : SEND_ANY_ID;
    MPI_Request * status = &(handle->status);
    DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] posting isend for msgId %ld type %x to rank %d\n", (int) self->pd->myLocation, message->msgId, message->type, targetRank);
    int res = MPI_Isend(message, bufferSize, datatype, targetRank, tag, comm, status);
    if (res == MPI_SUCCESS) {
        mpiComm->outgoing->pushFront(mpiComm->outgoing, handle);
        *id = mpiId;
    }

    return res;
}

u8 MPICommPollMessage_RL2(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                          u64* bufferSize, u32 properties, u32 *mask) {
    // NOTE: If one-way were tracked by the comm-api we would need to have
    // this sort of loop there to notify message status.
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);
    ocrPolicyDomain_t * pd = self->pd;
    //DIST-TODO check there are no post for responses
    linkedlist_t * outbox = mpiComm->outgoing;
    // RL1 should only be outgoing one-way shutdown message
    while(!outbox->isEmpty(outbox)) {
        iterator_t * outgoingIt = outbox->iterator(outbox);
        while (outgoingIt->hasNext(outgoingIt)) {
            mpiCommHandle_t * handle = (mpiCommHandle_t *) outgoingIt->next(outgoingIt);
            int completed = 0;
            int ret = MPI_Test(&(handle->status), &completed, MPI_STATUS_IGNORE);
            ASSERT(ret == MPI_SUCCESS);
            if(completed) {
                u32 msgProperties = handle->properties;
                ASSERT (!(msgProperties & TWOWAY_MSG_PROP));
                ASSERT(msgProperties & PERSIST_MSG_PROP);
                pd->fcts.pdFree(pd, handle->msg);
                pd->fcts.pdFree(pd, handle);
                outgoingIt->removeCurrent(outgoingIt);
            }
        }
        //DIST-TODO make iterator a singleton for non-safe access and save on mem-alloc
        outgoingIt->destruct(outgoingIt);
    }
    return POLL_NO_MESSAGE;
}

u8 MPICommPollMessage_RL3(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                          u64* bufferSize, u32 properties, u32 *mask) {
    ocrPolicyDomain_t * pd = self->pd;
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);

    ASSERT(msg != NULL);
    ASSERT((*msg == NULL) && "MPI comm-layer cannot poll for a specific message");
    // Iterate over outgoing communications (mpi sends)
    iterator_t * outgoingIt = mpiComm->outgoing->iterator(mpiComm->outgoing);
    while (outgoingIt->hasNext(outgoingIt)) {
        mpiCommHandle_t * handle = (mpiCommHandle_t *) outgoingIt->next(outgoingIt);
        int completed = 0;
        int ret = MPI_Test(&(handle->status), &completed, MPI_STATUS_IGNORE);
        ASSERT(ret == MPI_SUCCESS);
        if(completed) {
            DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] successfully sent msgId %ld type %x\n", (int) self->pd->myLocation, handle->msg->msgId, handle->msg->type);
            u32 msgProperties = handle->properties;
            // By construction, either message are persistent in API's upper levels
            // or they've been made persistent on the send through a copy.
            ASSERT(msgProperties & PERSIST_MSG_PROP);
            // delete the message if one-way (request or response).
            // Otherwise message is used to store the reply
            if (!(msgProperties & TWOWAY_MSG_PROP)) {
                pd->fcts.pdFree(pd, handle->msg);
            }
            pd->fcts.pdFree(pd, handle);
            outgoingIt->removeCurrent(outgoingIt);
        }
    }
    //DIST-TODO make iterator a singleton for non-safe access and save on mem-alloc
    outgoingIt->destruct(outgoingIt);

    // Iterate over incoming communications (mpi recvs)
    iterator_t * incomingIt = mpiComm->incoming->iterator(mpiComm->incoming);
    bool debugIts = false;
    while (incomingIt->hasNext(incomingIt)) {
        debugIts = true;
        mpiCommHandle_t * handle = (mpiCommHandle_t *) incomingIt->next(incomingIt);
        int completed = 0;
        int ret = MPI_Test(&(handle->status), &completed, MPI_STATUS_IGNORE);
        ASSERT(ret == MPI_SUCCESS);
        if (completed) {
            ocrPolicyMsg_t * receivedMsg = handle->msg;
            u32 needRecvAny = (receivedMsg->type & PD_MSG_REQUEST);
            DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] Received a message of type %x with msgId %d \n",
                    (int) self->pd->myLocation, receivedMsg->type, (int) receivedMsg->msgId);
            // if request : msg may be reused for the response
            // if response: upper-layer must process and deallocate
            //DIST-TODO there's no convenient way to communicate that to upper layers
            *msg = receivedMsg;
            pd->fcts.pdFree(pd, handle);
            incomingIt->removeCurrent(incomingIt);
            incomingIt->destruct(incomingIt);
            if (needRecvAny) {
                // Receiving a request indicates a mpi recv any
                // has completed. Post a new one.
                postRecvAny(self);
            }
            return POLL_MORE_MESSAGE;
        }
    }
    ASSERT(debugIts != false); // There should always be an irecv any posted
    incomingIt->destruct(incomingIt);

    return POLL_NO_MESSAGE;
}

u8 MPICommPollMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                      u64* bufferSize, u32 properties, u32 *mask) {
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);
    switch(mpiComm->rl) {
        case 3:
            return MPICommPollMessage_RL3(self, msg, bufferSize, properties, mask);
        case 2: {
            // The comm-worker is draining all remaining communication out
            // of the scheduler, then calls poll to complete those.
            //DIST-TODO This RL's poll, block until all the work is completed so that
            //we guarantee the worker there's no outstanding comm
            u8 res = MPICommPollMessage_RL2(self, msg, bufferSize, properties, mask);
            ASSERT(res == POLL_NO_MESSAGE);
            mpiComm->rl_completed[2] = true;
            return res;
        }
        default:
            // nothing to do
            ASSERT(false && "Illegal RL reached in MPI-comm-platform pollMessage");
    }
    return POLL_NO_MESSAGE;
}

u8 MPICommWaitMessage(ocrCommPlatform_t *self, ocrPolicyMsg_t **msg,
                      u64* bufferSize, u32 properties, u32 *mask) {
    u8 ret = 0;
    do {
        ret = self->fcts.pollMessage(self, msg, bufferSize, properties, mask);
    } while(ret != POLL_MORE_MESSAGE);

    return ret;
}

//
// Comm-platform life-cycle
//

void MPICommBegin(ocrCommPlatform_t * self, ocrPolicyDomain_t * pd, ocrCommApi_t *commApi) {
    //Initialize base
    self->pd = pd;
    //DIST-TODO location: both commPlatform and worker have a location, are the supposed to be the same ?
    int nbRanks=0;
    int rank=0;
    MPI_Comm_size(MPI_COMM_WORLD, &nbRanks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    DPRINTF(DEBUG_LVL_VERB,"[%d][MPI] comm-platform starts\n",rank);
    pd->myLocation = (ocrLocation_t) rank+HACK_RANK_SHIFT;
    return;
}

void MPICommStart(ocrCommPlatform_t * self, ocrPolicyDomain_t * pd, ocrCommApi_t *commApi) {
    //DIST-TODO: this is all fine because there's a single communication worker per PD
    //Initialize mpi comm internal queues
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);
    mpiComm->msgId = 1;
    mpiComm->incoming = newLinkedList(pd);
    mpiComm->outgoing = newLinkedList(pd);
    // Default max size is customizable through setMaxExpectedMessageSize()
    mpiComm->maxMsgSize = sizeof(ocrPolicyMsg_t);

    // All-to-all neighbor knowledge
    int nbRanks;
    MPI_Comm_size(MPI_COMM_WORLD, &nbRanks);
    pd->neighborCount = nbRanks - 1;
    pd->neighbors = pd->fcts.pdMalloc(pd, sizeof(ocrLocation_t) * nbRanks);
    int myRank = (int) pd->myLocation;
    int i = 0;
    while(i < (nbRanks-1)) {
        pd->neighbors[i] = (((myRank+i+1)%nbRanks)+HACK_RANK_SHIFT);
        DPRINTF(DEBUG_LVL_VERB,"[%d] neighbors[%d] is %d\n", myRank, i, pd->neighbors[i]);
        i++;
    }

    // Post a recv any to start listening to incoming communications
    postRecvAny(self);
}

/**
 * @brief Stops the communication platform
 * Guarantees all outgoing sends and non-outstanding recv are done
 */
void MPICommStop(ocrCommPlatform_t * self) {
    ocrCommPlatformMPI_t * mpiComm = ((ocrCommPlatformMPI_t *) self);
    // Some other worker wants to stop the communication worker.
    //DIST-TODO: self stop for mpi-comm-platform i.e. called by comm-worker
    DPRINTF(DEBUG_LVL_VERB,"[%d] Calling MPI STOP %d\n",  (int) self->pd->myLocation, mpiComm->rl);
    switch(mpiComm->rl) {
        case 3:
            mpiComm->rl_completed[3] = true;
            mpiComm->rl = 2;
            // Transition to RL2:
            // At this point the comm-worker is in RL2 pulling and sending
            // all the remaining communication work from the scheduler.
            // There's no comm-platform code invocation in RL2.
        break;
        case 2:
            ASSERT(mpiComm->rl_completed[2]);
            mpiComm->rl = 1;
        break;
        case 1:
            //DIST-TODO check if we remove a RL
            mpiComm->rl_completed[1] = true;
            mpiComm->rl = 0;
        break;
        case 0:
            // RL0: just cleanup data-structure pdMalloc-ed
            //NOTE: incoming may have recvs any posted
            mpiComm->incoming->destruct(mpiComm->incoming);
            ASSERT(mpiComm->outgoing->isEmpty(mpiComm->outgoing));
            mpiComm->outgoing->destruct(mpiComm->outgoing);
        break;
        default:
            ASSERT(false && "Illegal RL reached in MPI-comm-platform stop");
    }
    DPRINTF(DEBUG_LVL_VERB,"[%d] Exiting Calling MPI STOP %d\n",  (int) self->pd->myLocation, mpiComm->rl);
}

void MPICommFinish(ocrCommPlatform_t *self) {
}

//
// Init and destruct
//

void MPICommDestruct (ocrCommPlatform_t * base) {
    //This should be called only once per rank and
    //by the same thread that did MPI_Init.
    platformFinalizeMPIComm();
    runtimeChunkFree((u64)base, NULL);
}

ocrCommPlatform_t* newCommPlatformMPI(ocrCommPlatformFactory_t *factory,
                                       ocrParamList_t *perInstance) {
    ocrCommPlatformMPI_t * commPlatformMPI = (ocrCommPlatformMPI_t*)
        runtimeChunkAlloc(sizeof(ocrCommPlatformMPI_t), NULL);
    //DIST-TODO what is a comm-platform location ? is it the same as the PD ?
    commPlatformMPI->base.location = ((paramListCommPlatformInst_t *)perInstance)->location;
    commPlatformMPI->base.fcts = factory->platformFcts;
    factory->initialize(factory, (ocrCommPlatform_t *) commPlatformMPI, perInstance);
    return (ocrCommPlatform_t*) commPlatformMPI;
}

/******************************************************/
/* MPI COMM-PLATFORM FACTORY                          */
/******************************************************/

void destructCommPlatformFactoryMPI(ocrCommPlatformFactory_t *factory) {
    runtimeChunkFree((u64)factory, NULL);
}

void initializeCommPlatformMPI(ocrCommPlatformFactory_t * factory, ocrCommPlatform_t * base, ocrParamList_t * perInstance) {
    initializeCommPlatformOcr(factory, base, perInstance);
    ocrCommPlatformMPI_t * mpiComm = (ocrCommPlatformMPI_t*) base;
    mpiComm->msgId = 1; // all recv any use id '0'
    mpiComm->incoming = NULL;
    mpiComm->outgoing = NULL;
    mpiComm->maxMsgSize = 0;
    int i = 0;
    while (i < (MPI_COMM_RL_MAX+1)) {
        mpiComm->rl_completed[i++] = false;
    }
    mpiComm->rl = MPI_COMM_RL_MAX;
}

ocrCommPlatformFactory_t *newCommPlatformFactoryMPI(ocrParamList_t *perType) {
    ocrCommPlatformFactory_t *base = (ocrCommPlatformFactory_t*)
        runtimeChunkAlloc(sizeof(ocrCommPlatformFactoryMPI_t), (void *)1);
    base->instantiate = &newCommPlatformMPI;
    base->initialize = &initializeCommPlatformMPI;

    base->destruct = FUNC_ADDR(void (*)(ocrCommPlatformFactory_t*), destructCommPlatformFactoryMPI);
    base->platformFcts.destruct = FUNC_ADDR(void (*)(ocrCommPlatform_t*), MPICommDestruct);
    base->platformFcts.begin = FUNC_ADDR(void (*)(ocrCommPlatform_t*, ocrPolicyDomain_t*,
                                                  ocrCommApi_t*), MPICommBegin);
    base->platformFcts.start = FUNC_ADDR(void (*)(ocrCommPlatform_t*,ocrPolicyDomain_t*,
                                                  ocrCommApi_t*), MPICommStart);
    base->platformFcts.stop = FUNC_ADDR(void (*)(ocrCommPlatform_t*), MPICommStop);
    base->platformFcts.finish = FUNC_ADDR(void (*)(ocrCommPlatform_t*), MPICommFinish);
    base->platformFcts.sendMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrLocation_t,
                                               ocrPolicyMsg_t*,u64,u64*,u32,u32), MPICommSendMessage);
    base->platformFcts.pollMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrPolicyMsg_t**,u64*,u32,u32*),
                                               MPICommPollMessage);
    base->platformFcts.waitMessage = FUNC_ADDR(u8 (*)(ocrCommPlatform_t*,ocrPolicyMsg_t**,u64*,u32,u32*),
                                               MPICommWaitMessage);

    return base;
}

#endif /* ENABLE_COMM_PLATFORM_MPI */
