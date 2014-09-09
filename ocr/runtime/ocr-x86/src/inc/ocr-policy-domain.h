/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_POLICY_DOMAIN_H_
#define OCR_POLICY_DOMAIN_H_

#include "ocr-allocator.h"
#include "ocr-comm-api.h"
#include "ocr-datablock.h"
#include "ocr-event.h"
#include "ocr-guid.h"
#include "ocr-scheduler.h"
#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif
#include "ocr-task.h"
#include "ocr-types.h"
#include "ocr-worker.h"

#include "experimental/ocr-placer.h"
#include "experimental/ocr-tuning.h"

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListPolicyDomainFact_t {
    ocrParamList_t base;
} paramListPolicyDomainFact_t;

typedef struct _paramListPolicyDomainInst_t {
    ocrParamList_t base;
    ocrLocation_t location;
} paramListPolicyDomainInst_t;


/******************************************************/
/* OCR POLICY DOMAIN INTERFACE                        */
/******************************************************/

// WARNING: Keep this list in sync with ocr-policy-msg-list.h

/**< Invalid message */
#define PD_MSG_INVAL            0

/* Bit structure for types of message:
 *     - Bottom 12 bits encode whether the message is about computation,
 *       memory, etc (001, 002, 004, ...)
 *     - Next 4 bits indicate the operation ID (sequential number 1, 2, 3...)
 *     - 0x100000 and 0x200000 indicate query/response
 */
/**< AND with this and if result non-null, movable-memory operation.
 * Generally, these will be DBs but it could be something else too
 * (runtime DBs, etc). For pure memory chunks (directly out
 * of allocators), use PD_MSG_MEM_OP
 */
#define PD_MSG_DB_OP            0x001
/**< Create a memory area (allocate) */
#define PD_MSG_DB_CREATE        0x1001
/**< Destroy a DB (orginates from PD<->PD) */
#define PD_MSG_DB_DESTROY       0x2001
/**< Acquires a DB */
#define PD_MSG_DB_ACQUIRE       0x3001
/**< Releases a DB */
#define PD_MSG_DB_RELEASE       0x4001
/**< Frees a DB (the last free may trigger a destroy) */
#define PD_MSG_DB_FREE          0x5001

/**< AND with this and if the result non-null, memory chunks
 * related operation (goes directly to allocators) */
#define PD_MSG_MEM_OP                 0x002
#define PD_MSG_MEM_ALLOC              0x1002
/**< De-allocates a chunk of memory (through an allocator).
 * This is called internally */
#define PD_MSG_MEM_UNALLOC            0x2002


/**< AND with this and if result non-null, work/task related operation.
 * Generally, these will be EDTs but it could be something else too
 */
#define PD_MSG_WORK_OP          0x004
/**< Create an EDT */
#define PD_MSG_WORK_CREATE      0x1004
/**< Execute this EDT (originates from PD<->PD) */
#define PD_MSG_WORK_EXECUTE     0x2004
/**< Destroy an EDT (originates from PD<->PD) */
#define PD_MSG_WORK_DESTROY     0x3004

/**< AND with this and if result non-null, EDT-template related operation */
#define PD_MSG_EDTTEMP_OP       0x008
/**< Create an EDT template */
#define PD_MSG_EDTTEMP_CREATE   0x1008
/**< Destroy an EDT template */
#define PD_MSG_EDTTEMP_DESTROY  0x2008

/**< AND with this and if result non-null, Event related operation */
#define PD_MSG_EVT_OP           0x010
/**< Create an event */
#define PD_MSG_EVT_CREATE       0x1010
/**< Destroy an event */
#define PD_MSG_EVT_DESTROY      0x2010
/**< Get the entity that satisfied the event (originates from PD<->PD) */
#define PD_MSG_EVT_GET          0x3010

/**< AND with this and if result non-null, GUID related operations */
#define PD_MSG_GUID_OP          0x020
/**< Either associate a known u64 value with a created GUID OR
 * create a GUID metadata and associate that address with a GUID */
#define PD_MSG_GUID_CREATE      0x1020

/**< Gets information about the GUID */
#define PD_MSG_GUID_INFO        0x2020

/**< Request a copy of the GUID's metadata */
#define PD_MSG_GUID_METADATA_CLONE  0x3020

/**< Release the GUID (destroy the association between the u64
 * value and the GUID and optionally destroy the associated
 * metadata */
#define PD_MSG_GUID_DESTROY     0x4020
// TODO: Add stuff about GUID reservation

/**< AND with this and if result non-null, GUID distribution related
 * operation (taking/giving EDTs, DBs, events, etc)
 */
#define PD_MSG_COMM_OP          0x040
/**< Request for a GUID (ie: the caller wants the callee
 * to give it the GUID(s) requested (pull model) */
#define PD_MSG_COMM_TAKE        0x1040
/**< Request for a GUID to be put (ie: the caller wants the
 * callee to accept the GUID(s) given (push model) */
#define PD_MSG_COMM_GIVE        0x2040

/**< AND with this and if result non-null, dependence related
 * operation
 */
#define PD_MSG_DEP_OP           0x080
/**< Add a dependence. This will call registerSignaler and registerWaiter
 * on both sides of the dependence as appropriate*/
#define PD_MSG_DEP_ADD          0x1080

/**< Register a signaler on a waiter. This is called internally by the PD
 * as a result of PD_MSG_DEP_ADD (potentially). DEP_ADD is effectively
 * a REGSIGNALER and a REGWAITER.
 */
#define PD_MSG_DEP_REGSIGNALER  0x2080

/**< Register a waiter on a signaler. This is called internally by the PD
 * as a result of PD_MSG_DEP_ADD (potentially). DEP_ADD is effectively
 * a REGSIGNALER and a REGWAITER.
 */
#define PD_MSG_DEP_REGWAITER    0x3080

/**< Satisfy a dependence. A user can satisfy an event but the general
 * case is that a signaler satisfies its waiter(s)
 */
#define PD_MSG_DEP_SATISFY      0x4080

/**< Unregister a signaler on a waiter. This is called internally
 * when an event is destroyed for example */
#define PD_MSG_DEP_UNREGSIGNALER 0x5080

/**< Unregister a signaler on a waiter */
#define PD_MSG_DEP_UNREGWAITER  0x6080

/**< Adds a "dynamic" dependence (creation of a DB mostly) */
#define PD_MSG_DEP_DYNADD       0x7080

/**< Removes a potential dynamic dependence */
#define PD_MSG_DEP_DYNREMOVE    0x8080

/**< AND with this and if result non-null, low-level OS operation */
#define PD_MSG_SAL_OP           0x100
/**< Print operation */
#define PD_MSG_SAL_PRINT        0x1100
/**< Read operation */
#define PD_MSG_SAL_READ         0x2100
/**< Write operation */
#define PD_MSG_SAL_WRITE        0x3100
// TODO: Possibly add open, close, seek
/**< Abort/exit the runtime */
#define PD_MSG_SAL_TERMINATE    0x4100

/**< And with this and if the result is non-null, PD management operations */
#define PD_MSG_MGT_OP           0x200
/**< Shutdown operation (indicates the PD should
 * shut-down. Another call "finish" will actually
 * cause their destruction */
#define PD_MSG_MGT_SHUTDOWN     0x1200

/**< Finish operation (indicates the PD should
 * destroy itself. The existence of other PDs can
 * no longer be assumed */
#define PD_MSG_MGT_FINISH       0x2200

/**< Register a policy-domain with another
 * one. This registration is one way (ie: the
 * source of this call already knows of the destination
 * since it is sending it a message) */
#define PD_MSG_MGT_REGISTER     0x3200

/**< Opposite of register */
#define PD_MSG_MGT_UNREGISTER   0x4200

/**< For a worker to request the policy-domain to monitor an operation progress */
#define PD_MSG_MGT_MONITOR_PROGRESS 0x5200


#ifdef OCR_ENABLE_STATISTICS
// TODO: Add statistics messages
// The entire statistics framework will have to
// be reseen to deal with messages as opposed
// to function calls
// For later
#endif

/**< And this to just get the type of the message without
 * the upper flags
 */
#define PD_MSG_TYPE_ONLY        0xFFFFFULL

#define PD_MSG_META_ONLY        0xFFF00000ULL

/**< Defines that the message is a query (non-answered) */
#define PD_MSG_REQUEST          0x100000ULL
/**< Defines that the message is a response */
#define PD_MSG_RESPONSE         0x200000ULL
/**< Defines if the message requires a return message */
#define PD_MSG_REQ_RESPONSE     0x400000ULL


#define PD_MSG_ARG_NAME_SUB(ID) _data_##ID
#define PD_MSG_STRUCT_NAME(ID) PD_MSG_ARG_NAME_SUB(ID)


#define PD_MSG_FIELD_FULL_SUB(ptr, type, field) ptr->args._data_##type.field
#define PD_MSG_FIELD_FULL(ptr, type, field) PD_MSG_FIELD_FULL_SUB((ptr), type, field)
#define PD_MSG_FIELD(field) PD_MSG_FIELD_FULL(PD_MSG, PD_TYPE, field)

// TODO: Check if this works with packing and what not. In particular it assumes that
// all union members start at the start of the union.
#define PD_MSG_SIZE_FULL_SUB(type) \
    ((u64)(&(((struct _ocrPolicyMsg_t*)0)->args)) + sizeof(((struct _ocrPolicyMsg_t*)0)->args._data_##type))
#define PD_MSG_SIZE_FULL(type) PD_MSG_SIZE_FULL_SUB(type)
#define PD_MSG_SIZE PD_MSG_SIZE_FULL(PD_TYPE)

#define PD_MSG_RESET_FULL_SUB(ptr) do { ptr->destLocation = UNDEF_LOCATION; ptr->msgId = 0; } while(0)
#define PD_MSG_RESET_FULL(ptr) PD_MSG_RESET_FULL_SUB((ptr))
#define PD_MSG_RESET PD_MSG_RESET_FULL(PD_MSG)

struct _ocrPolicyDomain_t;

/**
 * @brief Structure describing a "message" that is used to communicate between
 * policy domains in an asynchronous manner
 *
 * Communication between policy domains is always assumed to be asynchronous
 * where the requester (source) does not execute the code of the remote
 * policy domain (it is therefore like a remote procedure call). Specific
 * implementations may choose a synchronous communication but asynchronous
 * communication is assumed.
 *
 * This class defines the "message" that will be sent between policy
 * domains. All messages to/from policy domains are encapsulated
 * in this format (even if synchronous)
 */
typedef struct _ocrPolicyMsg_t {
    u32 type;                   /**< Type of the message. Also includes if this
                                 * is a request or a response */
    u32 size;                   /**< Useful size of this message in bytes. This is the number
                                 * of bytes in this message buffer that are useful to
                                 * transmit. This can be reset by ocrPolicyMsgGetMsgSize to
                                 * just the size of the main content of the message. */
    ocrLocation_t srcLocation;  /**< Source of the message
                                 * (location making the request) */
    ocrLocation_t destLocation; /**< Destination of the message
                                 * (location processing the request) */
    u64 msgId;                  /**< Implementation specific ID identifying
                                 * this message (if required) */

    /* The following rules apply to all fields in the message:
     *     - All ocrFatGuid_t are in/out parameters in the sense
     *       that if they come in with only the GUID information
     *       set, they may get fully resolved and the metaDataPtr
     *       may be set properly on return. If on return metaDataPtr is NULL,
     *       it means that the GUID metadata cannot be accessed from the PD
     *     - All 'guid' parameters for the creation of objects are in/out:
     *       If they are not NULL_GUID on input, the runtime will use the GUID
     *       specified for the object.
     *     - The 'properties' field is input only.
     *     - The 'returnDetail' field is output only and on return it contains
     *       the error code of any deeper call the processMessage function makes.
     *       The return value of processMessage is its own error code (ie: not
     *       the internal message's processing)
     */
    union {
        struct {
            ocrFatGuid_t guid;            /**< In/Out: GUID of the created
                                          * memory segment (usually a DB) */
            ocrFatGuid_t edt;             /** In: EDT doing the creation */
            void* ptr;                    /**< Out: Address of created DB */
            u64 size;                     /**< In: Size of the created DB */
            ocrFatGuid_t affinity;        /**< In: Affinity group for the DB */
            u32 properties;               /**< In: Properties for creation */
            u32 returnDetail;             /**< Out: Success or error code */
            ocrDataBlockType_t dbType;    /**< In: Type of memory requested */
            ocrInDbAllocator_t allocator; /**< In: In-DB allocator */
        } PD_MSG_STRUCT_NAME(PD_MSG_DB_CREATE);

        struct {
            ocrFatGuid_t guid;         /**< In: GUID of the DB to destroy */
            ocrFatGuid_t edt;          /**< In: EDT doing the destruction */
            u32 properties;            /**< In: properties for the destruction */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DB_DESTROY);

        struct {
            ocrFatGuid_t guid;         /**< In: GUID of the DB to acquire */
            ocrFatGuid_t edt;          /**< In: EDT doing the acquiring */
            void* ptr;                 /**< Out: Pointer to the acquired memory */
            u64 size;                  /**< Out: Size of the acquired memory */
            u32 properties;            /**< In: Properties for acquire. Bit 0: 1 if runtime acquire */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DB_ACQUIRE);

        struct {
            ocrFatGuid_t guid;         /**< In: GUID of the DB to release */
            ocrFatGuid_t edt;          /**< In: GUID of the EDT doing the release */
            void* ptr;                 /**< In: Optionally provide pointer to the released memory */
            u64 size;                  /**< In: Optionally provide size to the released memory */
            u32 properties;            /**< In: Properties of the release: Bit 0: 1 if runtime release */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DB_RELEASE);

        struct {
            ocrFatGuid_t guid;         /**< In: GUID of the DB to free */
            ocrFatGuid_t edt;          /**< In: GUID of the EDT doing the free */
            u32 properties;            /**< In: Properties of the free */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DB_FREE);

        struct {
            u64 size;                  /**< In: Size of memory chunk to allocate */
            void* ptr;                 /**< Out: Pointer of the allocated chunk */
            ocrFatGuid_t allocatingPD; /**< Out: GUID of the PD that owns the allocator */
            ocrFatGuid_t allocator;    /**< Out: GUID of the allocator that provided this memory */
            ocrMemType_t type;         /**< In: Type of memory requested */
            u32 properties;            /**< In: Properties for the allocation */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MEM_ALLOC);

        struct {
            void* ptr;                 /**< In: Pointer to the memory to free */
            ocrFatGuid_t allocatingPD; /**< In: GUID of the PD that owns the allocator; not necessarily
                                        * required for all ocrMemType_t (in particular GUID_MEMTYPE) */
            ocrFatGuid_t allocator;    /**< In: GUID of the allocator that gave the memory; same comment
                                        * as above */
            ocrMemType_t type;
            u32 properties;            /**< In: Properties for the free */
            u32 returnDetail;          /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MEM_UNALLOC);

        struct {
            ocrFatGuid_t guid;         /**< In/Out: GUID of the EDT/Work
                                        * to create */
            ocrFatGuid_t templateGuid; /**< In: GUID of the template to use */
            ocrFatGuid_t affinity;     /**< In: Affinity for this EDT */
            ocrFatGuid_t outputEvent;  /**< In/Out: If UNINITIALIZED_GUID on input,
                                       * will contain the output event to wait for for this
                                       * EDT. If NULL_GUID, no event will be created
                                       * and returned. */
            u64 *paramv;               /**< In: Parameters for this EDT */
            u32 paramc;                /**< In/out: Number of parameters; on out returns real number
                                        * in case of EDT_PARAM_DEF as input for example */
            u32 depc;                  /**< In/out: Number of dependence slots; same comment as above */
            ocrFatGuid_t * depv;       /**< In: Dependences for this EDT */
            u32 properties;            /**< In: properties for the creation */
            u32 returnDetail;          /**< Out: Success or error code */
            ocrWorkType_t workType;    /**< In: Type of work to create */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is creating work */
        } PD_MSG_STRUCT_NAME(PD_MSG_WORK_CREATE);

        struct {
            ocrFatGuid_t guid; /**< In: GUID of the EDT to execute */
            u32 properties;    /**< In: Properties for the execution */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_WORK_EXECUTE);

        struct {
            ocrFatGuid_t guid; /**< In: GUID of the EDT to destroy */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is destroying work */
            u32 properties;    /**< In: properties for the destruction */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_WORK_DESTROY);

        struct {
            ocrFatGuid_t guid;     /**< In/Out: GUID of the EDT template */
            ocrEdt_t funcPtr;      /**< In: Function to execute for this EDT */
            u32 paramc;            /**< In: Number of parameters for EDT */
            u32 depc;              /**< In: Number of dependences for EDT */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
            const char * funcName; /**< In: Debug help: user identifier */
            u64 funcNameLen;       /**< In: Number of characters (excluding '\0') in funcName */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is creating template */
        } PD_MSG_STRUCT_NAME(PD_MSG_EDTTEMP_CREATE);

        struct {
            ocrFatGuid_t guid; /**< In: GUID of the EDT template to destroy */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is destroying the template */
            u32 properties;    /**< In: properties for the destruction */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_EDTTEMP_DESTROY);

        struct {
            ocrFatGuid_t guid;    /**< In/Out: GUID of the event to create */
            u32 properties;       /**< In: Properties for this creation */
            u32 returnDetail;     /**< Out: Success or error code */
            ocrEventTypes_t type; /**< Type of the event created: Bit 0: 1 if event takes an argument */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is creating event */
        } PD_MSG_STRUCT_NAME(PD_MSG_EVT_CREATE);

        struct {
            ocrFatGuid_t guid; /**< In: GUID of the event to destroy */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is destroying event */
            u32 properties;    /**< In: properties for the destruction */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_EVT_DESTROY);

        struct {
            ocrFatGuid_t guid; /**< In: GUID of the event to get the data from */
            ocrFatGuid_t data; /**< Out: GUID of the DB used to satisfy the event */
            u32 properties;    /**< In: Properties for the get */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_EVT_GET);

        struct {
            ocrFatGuid_t guid; /**< In/Out:
                               *  In: The metaDataPtr field contains the value
                               *  to associate with the GUID or NULL if the metadata
                               *  memory needs to be created
                               *  Out: The guid field contains created GUID */
            u64 size;          /**< In: If metaDataPtr is NULL on input, contains the
                                *   size needed to contain the metadata. Otherwise ignored */
            ocrGuidKind kind;  /**< In: Kind of the GUID to create */
            u32 properties;    /**< In: Properties for the creation. */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_GUID_CREATE);

        struct {
            ocrFatGuid_t guid; /**< In/Out:
                                * In: The GUID field should be set to the GUID
                                * whose information is needed
                                * Out: Fully resolved information */
            ocrGuidKind kind;  /**< Out: Contains the type of the GUID */
            ocrLocation_t location; /**< Out: Contains the location of the GUID */
            u32 properties;    /**< In: Properties for the info. See ocrGuidInfoProp_t */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_GUID_INFO);

        struct {
            ocrFatGuid_t guid; /**< In/Out:
                                * In: The GUID we request a metadata copy
                                * Out: The GUID and pointer to the cloned metadata */
            u64 size;          /**< Out: Size of the metadata that was cloned */
        } PD_MSG_STRUCT_NAME(PD_MSG_GUID_METADATA_CLONE);

        struct {
            ocrFatGuid_t guid; /**< In: GUID to destroy */
            u32 properties;    /**< In: Properties for the destruction:
                                * Bit 0: If 1, metadata area is "freed" */
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_GUID_DESTROY);

        struct {
            ocrFatGuid_t *guids; /**< In/Out: GUID(s) of the work/DB/etc taken:
                                 * Input (optional): GUID(s) requested
                                 * Output: GUID(s) given to the caller
                                 * by the callee */
            u64 extra;           /**< Additional information on the take (for eg: function
                                  * pointer to use to execute the returned EDTs */
            u32 guidCount;       /**< In/Out: Number of GUID(s) in guids. As input,
                                  * number of GUIDs if guids has requested GUIDs or
                                  * maximum number of GUIDs requested. */
            u32 properties;      /**< In: properties for the take; TODO: define some for extra */
            u32 returnDetail;    /**< Out: Success or error code */
            ocrGuidKind type;    /**< In: Kind of GUIDs requested */
            // TODO: Add something about cost/choice heuristic
        } PD_MSG_STRUCT_NAME(PD_MSG_COMM_TAKE);

        struct {
            ocrFatGuid_t *guids; /**< In/Out: GUID(s) of the work/DB/etc given:
                                 * Input: GUID(s) the caller wants to hand-off
                                 * to the callee
                                 * Output (optional): GUID(s) NOT accepted
                                 * by callee */
            u32 guidCount;       /**< Number of GUID(s) in guids */
            u32 properties;      /**< In: properties for the give */
            u32 returnDetail;    /**< Out: Success or error code */
            ocrGuidKind type;    /**< In: Kind of GUIDs given */
            // TODO: Do we need something about cost/choice heuristic here
        } PD_MSG_STRUCT_NAME(PD_MSG_COMM_GIVE);

        struct {
            ocrFatGuid_t source; /**< In: Source of the dependence */
            ocrFatGuid_t dest;   /**< In: Destination of the dependence */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is adding dep */
            u32 slot;            /**< In: Slot of dest to connect the dep to */
            u32 properties;      /**< In: Properties. Lower 3 bits are access modes */
            u32 returnDetail;    /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_ADD);

        struct {
            ocrFatGuid_t signaler;  /**< In: Signaler to register */
            ocrFatGuid_t dest;      /**< In: Object to register the signaler on */
            u32 slot;               /**< In: Slot on dest to register the signaler on */
            ocrDbAccessMode_t mode; /**< In: Access mode for the dependence's datablock */
            u32 properties;         /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_REGSIGNALER);

        struct {
            ocrFatGuid_t waiter;   /**< In: Waiter to register */
            ocrFatGuid_t dest;     /**< In: Object to register the waiter on */
            u32 slot;              /**< In: The slot on waiter that will be notified */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_REGWAITER);

        struct {
            ocrFatGuid_t guid;    /**< In: GUID of the event/task to satisfy */
            ocrFatGuid_t payload; /**< In: GUID of the "payload" to satisfy the
                                   * event/task with (a DB usually). */
            ocrFatGuid_t currentEdt;   /**< In: EDT that is satisfying dep */
            u32 slot;             /**< In: Slot to satisfy the event/task on */
            u32 properties;       /**< In: Properties for the satisfaction */
            u32 returnDetail;     /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_SATISFY);

        struct {
            ocrFatGuid_t signaler; /**< In: Signaler to unregister */
            ocrFatGuid_t dest;     /**< In: Object to unregister the signaler on */
            u32 slot;              /**< In: Slot on dest to unregister the signaler on */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_UNREGSIGNALER);

        struct {
            ocrFatGuid_t edt;      /**< In: EDT adding the dynamic dependence (to itself only) */
            ocrFatGuid_t db;       /**< In: DB dynamically acquired */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_DYNADD);

        struct {
            ocrFatGuid_t edt;      /**< In: EDT removing the dynamic dependence (to itself only) */
            ocrFatGuid_t db;       /**< In: DB dynamically being removed */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_DYNREMOVE);

        struct {
            ocrFatGuid_t waiter;   /**< In: Waiter to unregister */
            ocrFatGuid_t dest;     /**< In: Object to unregister the waiter on */
            u32 slot;              /**< In: The slot on waiter that will be notified */
            u32 properties;        /**< In: Properties */
            u32 returnDetail;      /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_DEP_UNREGWAITER);

        struct {
            const char* buffer;  /**< In: Character string to print (including NULL terminator) */
            u64 length;          /**< In: Length to print (including NULL terminator) */
            u32 properties;      /**< In: Properties for the print */
            u32 returnDetail;    /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_SAL_PRINT);

        struct {
            char* buffer;       /**< In/Out: Buffer to read into */
            u64 length;         /**< In/Out: On input contains maximum length
                                 * of buffer, on output, contains length read */
            u64 inputId;        /**< In: Identifier for where to read from */
            u32 properties;     /**< In: Properties for the read */
            u32 returnDetail;   /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_SAL_READ);

        struct {
            const char* buffer; /**< In: Buffer to write */
            u64 length;         /**< In/Out: Number of bytes to write. On return contains
                                 * bytes NOT written */
            u64 outputId;       /**< In: Identifier for where to write to */
            u32 properties;     /**< In: Properties for the write */
            u32 returnDetail;   /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_SAL_WRITE);

        struct {
            u64 errorCode;     /**< In: Error code if applicable */
            u32 properties;    /**< For now: 0 if normal exit, 1 if abort, 2 if assert*/
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_SAL_TERMINATE);
        struct {
            ocrFatGuid_t currentEdt;   /**< In: EDT that is calling shutdown */
            u32 properties;
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MGT_SHUTDOWN);

        struct {
            u32 properties;
            u32 returnDetail;  /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MGT_FINISH);

        struct {
            ocrLocation_t neighbor; /**< In: Neighbor registering */
            // TODO: Add things having to do with cost and relationship
            u32 properties;
            u32 returnDetail;       /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MGT_REGISTER);

        struct {
            ocrLocation_t neighbor; /**< In: Neighbor unregistering */
            u32 properties;
            u32 returnDetail;       /**< Out: Success or error code */
        } PD_MSG_STRUCT_NAME(PD_MSG_MGT_UNREGISTER);

        struct {
            void * monitoree;
            u32 properties; /**< In: first eight bits is type of monitoree */
        } PD_MSG_STRUCT_NAME(PD_MSG_MGT_MONITOR_PROGRESS);
    } args;
    char _padding[64]; // REC: HACK to be able to fit everything in messages!!
} ocrPolicyMsg_t;

/**
 * @brief Structure describing a message "handle" that is
 * used to keep track of the status of a communication.
 *
 * The handle is always created by the communication layer (comm-worker)
 * and destroyed by the user (caller of sendMessage, waitMessage...) using
 * the provided 'destruct' function.
 *
 * The response message will always be contained in handle->response.
 *
 * If 'msg' is non-NULL after a successful poll (handle->status = HDL_RESPONSE_OK),
 * or a wait, this serves as a reminder to the caller that the message
 * was passed in with PERSIST_MESSAGE_PROP (pointer to original message).
 *
 * The handle needs to be freed by the caller using the 'destruct' function.
 *
 * @see sendMessage() in ocr-comm-worker.h for more detail
 */
typedef struct _ocrMsgHandle_t {
    ocrPolicyMsg_t * msg;           /**< The message associated with the communication
                                       the handle represents. */
    ocrPolicyMsg_t * response;      /**< The response (if applicable) */
    ocrMsgHandleStatus_t status;   /**< Status of this handle. See ocrMsgHandleState */
    void (*destruct)(struct _ocrMsgHandle_t * self); /**< Destructor for this
                                                       * instance of the message
                                                       * handle */
    ocrCommApi_t* commApi;         /**< Pointer to the comm API that manages this handle */
    u64 properties;                /**< Implementation defined properties */
} ocrMsgHandle_t;

typedef struct _ocrPolicyDomainFcts_t {
    /**
     * @brief Destroys (and frees any associated memory) this
     * policy domain
     *
     * Call when the policy domain has stopped executing to free
     * any remaining memory.
     *
     * @param self                This policy domain
     */
    void (*destruct)(struct _ocrPolicyDomain_t *self);

    void (*begin)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Starts this policy domain
     *
     * This starts the portion of OCR that manages the resources contained
     * in this policy domain.
     *
     * @param self                This policy domain
     */
    void (*start)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Stops this policy domain
     *
     * This stops the portion of OCR that manages the resources
     * contained in this policy domain. The policy domain will also stop
     * responding to requests from other policy domains. Called before finish
     *
     * @param self                This policy domain
     */
    void (*stop)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Finish the execution of the policy domain
     *
     * Ask the policy domain to wrap up currently executing
     * task and shutdown workers. Finish is called after stop
     *
     * @param self                This policy domain
     */
    void (*finish)(struct _ocrPolicyDomain_t *self);

    /**
     * @brief Requests for the handling of the request msg
     *
     * This function can be called either by user code (for when the user code
     * requires runtime services) or internally by the runtime (when a message
     * has been received via CT.pollMessage for example)
     *
     * All code executed by processMessage until it reaches a sendMessage
     * is executed synchronously and inside the same address space.
     *
     * @param[in]     self       This policy domain
     * @param[in/out] msg        Message to process. Response will be
     *                           contained in msg if a response i
     *                           required and isBlocking is true. In all
     *                           cases, the message pointer must be
     *                           freed by the caller
     * @param[in]     isBlocking True if the processing of the message
     *                           need to complete before returning
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*processMessage)(struct _ocrPolicyDomain_t *self, ocrPolicyMsg_t *msg,
                         u8 isBlocking);

    /**
     * @brief Send a message outside of the policy domain.
     * This API can be used by any client of the policy domain and
     * will call into the correct comm-api sendMessage to actually
     * send the message.
     *
     * See ocr-comm-api.h for a detailed description
     * @param[in] self          This policy-domain
     * @param[in] target        Where to send the message
     * @param[in/out] handle    Handle for the message
     * @param[in] properties    Properties for the send
     * @return 0 on success and a non-zero error code
     */
    u8 (*sendMessage)(struct _ocrPolicyDomain_t* self, ocrLocation_t target, ocrPolicyMsg_t *message,
                      ocrMsgHandle_t **handle, u32 properties);

    /**
     * @brief Non-blocking check for incoming messages
     * This API can be used by any client of the policy domain and
     * will call into the correct comm-api pollMessage to actually
     * poll for a message.
     *
     * See ocr-comm-api.h for a detailed description
     * @param[in] self          This policy-domain
     * @param[in/out] handle    Handle for the message
     * @return 0 on success and a non-zero error code
     */
    u8 (*pollMessage)(struct _ocrPolicyDomain_t *self, ocrMsgHandle_t **handle);

    /**
     * @brief Blocking check for incoming messages
     * This API can be used by any client of the policy domain and
     * will call into the correct comm-api waitMessage to actually
     * wait for a message.
     *
     * See ocr-comm-api.h for a detailed description
     * @param[in] self          This policy-domain
     * @param[in/out] handle    Handle for the message
     * @return 0 on success and a non-zero error code
     */
    u8 (*waitMessage)(struct _ocrPolicyDomain_t *self, ocrMsgHandle_t **handle);

    /**
     * @brief Policy-domain only allocation.
     *
     * Memory allocated with this call can only be used within the
     * policy domain and must be freed using pdFree(). Memory allocated
     * with pdMalloc is meant to be private to the policy domain
     *
     * @param size  Number of bytes to allocate
     * @return A pointer to the allocated space
     */
    void* (*pdMalloc)(struct _ocrPolicyDomain_t *self, u64 size);

    /**
     * @brief Policy-domain only free.
     *
     * Used to free memory allocated with pdMalloc
     *
     * @param addr Address to free
     */
    void (*pdFree)(struct _ocrPolicyDomain_t *self, void* addr);

#ifdef OCR_ENABLE_STATISTICS
    ocrStats_t* (*getStats)(struct _ocrPolicyDomain_t *self);
#endif
} ocrPolicyDomainFcts_t;

/**
 * @brief A policy domain is OCR's way of dividing up the runtime in scalable
 * chunks
 *
 * A policy domain is considered a 'synchronous' section of the runtime. In
 * other words, if a given worker makes a call into its policy domain, that call
 * will be processed synchronously by that worker (and therefore by the
 * compute target and platform running the worker) as long as it stays within
 * the policy domain. When the call requires processing outside the policy
 * domain, an asynchronous one-way 'message' will be sent to the other policy
 * domain (this is similar to a remote procedure call). Implementation
 * would determine if the RPC call is actually executed asynchronously by
 * another worker or executed synchronously by the same worker but it will
 * always be treated as an asynchronous call. Since the user API is
 * synchronous, if no response is needed, the call into the policy domain
 * can then return to the caller. However, if a response is required, a
 * worker specific function is called informing it that it needs to
 * 'wait-for-network' (or something like that). The exact implementation
 * will determine whether the worker halts or does something else.
 *
 * Each policy domain contains the following:
 *     - 0 or more 'schedulers' which both schedule tasks and place data
 *     - 0 or more 'workers' on which EDTs are executed
 *     - 0 or more 'allocators' from which DBs are allocated/freed
 *
 * A policy domain will directly respond to requests emanating from user
 * code to:
 *     - create EDTs (using ocrEdtCreate())
 *     - create DBs (using ocrDbCreate())
 *     - create GUIDs
 *
 */
typedef struct _ocrPolicyDomain_t {

// CAUTION:  FIXME:  Any changes to this struct (including, for example, the sizeof(ocrFatGuid_t)
// will cause problems in the symbolic constants inside rmdkrnl/inc/rmd-bin-files.h

    ocrFatGuid_t fguid;                         /**< GUID for this policy */

    u64 schedulerCount;                         /**< Number of schedulers */
    u64 allocatorCount;                         /**< Number of allocators */
    u64 workerCount;                            /**< Number of workers */
    u64 commApiCount;                           /**< Number of comm APIs */
    u64 guidProviderCount;                      /**< Number of GUID providers */
    u64 taskFactoryCount;                       /**< Number of task factories */
    u64 taskTemplateFactoryCount;               /**< Number of task-template factories */
    u64 dbFactoryCount;                         /**< Number of data-block factories */
    u64 eventFactoryCount;                      /**< Number of event factories */

    ocrScheduler_t  ** schedulers;              /**< All the schedulers */
    ocrAllocator_t  ** allocators;              /**< All the allocators */
    ocrWorker_t     ** workers;                 /**< All the workers */
    ocrCommApi_t    ** commApis;                /**< All the communication interfaces */

    ocrTaskFactory_t  **taskFactories;          /**< Factory to produce tasks
                                                 * (EDTs) */

    ocrTaskTemplateFactory_t  ** taskTemplateFactories; /**< Factories to produce
                                                         * task templates */
    ocrDataBlockFactory_t ** dbFactories;       /**< Factories to produce
                                                 * data-blocks */
    ocrEventFactory_t ** eventFactories;        /**< Factories to produce events*/
    ocrGuidProvider_t ** guidProviders;         /**< GUID generators */
    ocrPlacer_t * placer;                       /**< Affinity and placement (work in progress) */

    // TODO: What to do about this?
    ocrCost_t *costFunction; /**< Cost function used to determine
                              * what to schedule/steal/take/etc.
                              * Currently a placeholder for future
                              * objective driven scheduling */

    ocrLocation_t myLocation;
    ocrLocation_t parentLocation;
    ocrLocation_t* neighbors;
    u32 neighborCount;

    ocrPolicyDomainFcts_t fcts;

    s8 * allocatorIndexLookup;                  /**< Allocator indices for each block agent, over each of 8 memory levels */
#ifdef OCR_ENABLE_STATISTICS
    ocrStats_t *statsObject;                    /**< Statistics object */
#endif

} ocrPolicyDomain_t;

/****************************************************/
/* OCR POLICY DOMAIN FACTORY                        */
/****************************************************/

typedef struct _ocrPolicyDomainFactory_t {
    /**
     * @brief Create a policy domain
     *
     * Allocates the required space for the policy domain
     * based on the counts passed as arguments. The 'schedulers',
     * 'workers', 'computes', 'workpiles', 'allocators' and 'memories'
     * data-structures must then be properly filled
     *
     * @param factory             This policy domain factory
     * @param configuration       An optional configuration
     * @param schedulerCount      The number of schedulers
     * @param workerCount         The number of workers
     * @param computeCount        The number of compute targets
     * @param workpileCount       The number of workpiles
     * @param allocatorCount      The number of allocators
     * @param memoryCount         The number of memory targets
     * @param taskFactory         The factory to use to generate EDTs
     * @param taskTemplateFactory The factory to use to generate EDTs templates
     * @param dbFactory           The factory to use to generate DBs
     * @param eventFactory        The factory to use to generate events
     * @param contextFactory      The factory to use to generate context
     * @param guidProvider        The provider of GUIDs for this policy domain
     * @param lockFactory         The factory to use to generate locks
     * @param atomicFactory       The factory to use to generate atomics
     * @param queueFactory        The factory to use to generate queues
     * @param costFunction        The cost function used by this policy domain
     */

    // TODO: Check with Bala on this. We may not need to pass anything
    // except the physical location of itself and its parent
    ocrPolicyDomain_t * (*instantiate) (struct _ocrPolicyDomainFactory_t *factory,
#ifdef OCR_ENABLE_STATISTICS
                                        ocrStats_t *statsObject,
#endif
                                        ocrCost_t *costFunction, ocrParamList_t *perInstance);
    void (*initialize) (struct _ocrPolicyDomainFactory_t *factory, ocrPolicyDomain_t* self,
#ifdef OCR_ENABLE_STATISTICS
                        ocrStats_t *statsObject,
#endif
                        ocrCost_t *costFunction, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrPolicyDomainFactory_t * factory);
    ocrPolicyDomainFcts_t policyDomainFcts;
} ocrPolicyDomainFactory_t;

void initializePolicyDomainOcr(ocrPolicyDomainFactory_t * factory, ocrPolicyDomain_t * self, ocrParamList_t *perInstance);


/****************************************************/
/* UTILITY FUNCTIONS                                */
/****************************************************/

typedef enum {
    MARSHALL_FULL_COPY,
    MARSHALL_DUPLICATE,
    MARSHALL_APPEND,
    MARSHALL_ADDL
} ocrMarshallMode_t;

/**
 * @brief Gets the effective size of the msg that
 * needs to be sent
 *
 * This size includes the actual payload + whatever
 * additional space is needed to store variable-size
 * arguments such as GUIDs, funcName etc.
 *
 * @param[in] msg               Message to analyze
 * @param[out] fullSize         Full size of the message (bytes)
 * @param[out] marshalledSize   Additional data that needs to be
 *                              fetched/marshalled (included in fullSize) (bytes)
 *
 * This call can be used on both a ocrPolicyMsg_t or a buffer
 * containing a marshalled ocrPolicyMsg_t. This will set the 'size'
 * field in the ocrPolicyMsg_t to fullSize - marshalledSize.
 *
 * @warning The setting of the size field may not be what you want;
 * for example, if the buffer contains the message AND the
 * marshalled content (for example if it was a MARSHALL_APPEND), this
 * will set the size to something SMALLER than what needs to be transmitted.
 *
 * @return 0 on success and a non-zero error code
 */
u8 ocrPolicyMsgGetMsgSize(struct _ocrPolicyMsg_t* msg, u64 *fullSize,
                          u64* marshalledSize);

/**
 * @brief Marshall everything needed to send the message over to a PD that
 * does not share the same address space
 *
 * This function performs one of three operations depending on the
 * mode:
 *   - MARSHALL_FULL_COPY: Fully copy and marshall msg into buffer. In
 *                         this case buffer and msg do not overlap. The
 *                         caller also ensures that buffer is at least
 *                         big enough to contain fullSize bytes as
 *                         returned by ocrPolicyMsgGetMsgSize. Assumes
 *                         that msg->size is properly set to (fullSize - marshalledSize)
 *   - MARSHALL_DUPLICATE: This is exactly like MARSHALL_FULL_COPY except
 *                         that the pointers are not encoded as offsets from the
 *                         start of the message. In other words, this mode
 *                         allows you to duplicate an ocrPolicyMsg_t fully
 *                         including the first level of pointers. This mode
 *                         is usually not used with an un-marshall.
 *   - MARSHALL_APPEND:    In this case (u64)buffer == (u64)msg and
 *                         marshalled values will be appended to the
 *                         end of the useful portion of msg. The caller
 *                         ensures that buffer is at least big enough to
 *                         contain fullSize bytes as returned by
 *                         ocrPolicyMsgGetMsgSize
 *   - MARSHALL_ADDL:      Marshall only neede portions (not already in msg)
 *                         into buffer. The caller ensures that buffer is
 *                         at least big enough to contain marshalledSize
 *                         as returned by ocrPolicyMsgGetMsgSize
 *
 * Note that in the case of MARSHALL_APPEND and MARSHALL_ADDL, the
 * msg itself is modified to encode offsets and positions for the marshalled
 * values so that it can be unmarshalled by ocrPolicyMsgUnMarshallMsg. In
 * all cases, size is set to what actually needs to be sent over:
 *   - For FULL_COPY, DUPLICATE or APPEND to fullSize
 *   - For ADDL to (fullSize - marshalledSize) since the additional information
 *     to transmit is in buffer and not in msg
 *
 * @param[in/out] msg      Message to marshall from
 * @param[in/out] buffer   Buffer to marshall to
 * @param[in] mode         One of MARSHALL_FULL_COPY, MARSHALL_APPEND, MARSHALL_ADDL
 * @return 0 on success and a non-zero error code
 */
u8 ocrPolicyMsgMarshallMsg(struct _ocrPolicyMsg_t* msg, u8* buffer, ocrMarshallMode_t mode);

/**
 * @brief Performs the opposite operation to ocrPolicyMsgMarshallMsg
 *
 * The inputs are the mainBuffer (containing the header of the messsage and
 * an optional additional buffer (obtained using the MARSHALL_ADDL mode).
 *
 * The significance of the modes here are:
 *   - MARSHALL_FULL_COPY: In this case, none of the buffers (mainBuffer,
 *                         addlBuffer, msg) overlap. The entire content
 *                         of the message and any additional structure
 *                         will be put in msg. The caller ensures that
 *                         the actual allocated size of msg is at least
 *                         fullSize bytes (as returned by ocrPolicyMsgGetMsgSize)
 *   - MARSHALL_APPEND:    In this case, (u64)msg == (u64)mainBuffer. In
 *                         most cases, this will only fix-up pointers but
 *                         may copy from addlBuffer into mainBuffer. For
 *                         example, the mashalling could have been
 *                         done using MARSHALL_ADDL and unmarshalling is
 *                         done with MARSHALL_APPEND.
 *                         The caller ensures that the actual allocated
 *                         size of msg is at least fullSize bytes (as
 *                         returned by ocrPolicyMsgGetMsgSize)
 *   - MARSHALL_ADDL:      Marshall the common part of the message into msg
 *                         and use pdMalloc for marshalledSize bytes (one
 *                         or more chunks totalling marshalledSize bytes as
 *                         returned by ocrPolicyMsgGetMsgSize)
 *
 * @param[in] mainBuffer   Buffer containing the common part of the message
 * @param[in] addlBuffer   Optional addtional buffer (if message was marshalled
 *                         using MARSHALL_ADDL). NULL otherwise (do not pass in
 *                         garbage!). It is assumed to have marshalledSize of useful
 *                         content
 * @param[out] msg         Message to create from mainBuffer and addlBuffer
 * @param[in] mode         One of MARSHALL_FULL_COPY, MARSHALL_APPEND, MARSHALL_ADDL
 *
 * @return 0 on success and a non-zero error code
 */
u8 ocrPolicyMsgUnMarshallMsg(u8* mainBuffer, u8* addlBuffer,
                             struct _ocrPolicyMsg_t* msg, ocrMarshallMode_t mode);


#define __GUID_END_MARKER__
#include "ocr-guid-end.h"
#undef __GUID_END_MARKER__

#endif /* OCR_POLICY_DOMAIN_H_ */
