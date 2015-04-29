/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_SCHEDULER_H__
#define __OCR_SCHEDULER_H__

#include "ocr-runtime-types.h"
#include "ocr-runtime-hints.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"
#include "ocr-guid-kind.h"
#include "ocr-scheduler-object.h"
#include "ocr-scheduler-heuristic.h"

struct _ocrPolicyDomain_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListSchedulerFact_t {
    ocrParamList_t base;
} paramListSchedulerFact_t;

typedef struct _paramListSchedulerInst_t {
    ocrParamList_t base;
} paramListSchedulerInst_t;


/****************************************************/
/* OCR SCHEDULER INFO                               */
/****************************************************/

typedef enum {
    OCR_SCHEDULER_OP_GIVE = 0,              /* Operation where a component is given to this scheduler */
    OCR_SCHEDULER_OP_TAKE,                  /* Operation where a request is made to take a component from this scheduler */
    OCR_SCHEDULER_OP_DONE,                  /* Operation to notify scheduler that a previous "taken" component is done processing */
    OCR_SCHEDULER_OP_DONE_TAKE,             /* Operation to notify a previous component is "done" with a request for a new take */
    NUM_SCHEDULER_OPS,
} ocrSchedulerOp_t;

typedef struct _ocrSchedulerOpArgs_t {
    ocrLocation_t loc;                      /* Location of client PD requesting scheduler invokation */
    u64 contextId;                          /* Sequential ID of 'loc' to allow for fast array index  */
    struct _ocrSchedulerObject_t *el;       /* schedulerObject used in this operation:
                                             * For GIVE:      IN: el is the schedulerObject to "give"
                                             * For TAKE:      IN: el is either NULL or is the allocation of the schedulerObject to "take"
                                             *                OUT: schedulerObject taken
                                             * For DONE:      IN: el is schedulerObject which is "done"
                                             * For DONE_TAKE: IN: el is schedulerObject which is "done"
                                             *                OUT: schedulerObject taken
                                             */
    ocrSchedulerObjectKind takeKind;        /* If a TAKE operation: kind of element to take */
    u32 takeCount;                          /* If a TAKE operation: number of elements requested */
} ocrSchedulerOpArgs_t;

/****************************************************/
/* OCR SCHEDULER                                    */
/****************************************************/

struct _ocrScheduler_t;
struct _ocrCost_t;
struct _ocrPolicyCtx_t;
struct _ocrMsgHandle_t;

typedef struct _ocrSchedulerOpFcts_t {
    /**
     * @brief Invokes the scheduler for a specific operation
     *
     * This call can be used to create EDTs/DBs/Events.
     * E.g: The data block create API can reason about
     * allocation based on scheduler information and
     * cost heuristics.
     *
     * @param self[in]           Pointer to this scheduler
     * @param opArgs[in]         Info about specific scheduling task
     * @param hints[in]          Hints for the create.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*invoke)(struct _ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints);

} ocrSchedulerOpFcts_t;

typedef struct _ocrSchedulerFcts_t {
    void (*destruct)(struct _ocrScheduler_t *self);

    /**
     * @brief Switch runlevel
     *
     * @param[in] self         Pointer to this object
     * @param[in] PD           Policy domain this object belongs to
     * @param[in] runlevel     Runlevel to switch to
     * @param[in] phase        Phase for this runlevel
     * @param[in] properties   Properties (see ocr-runtime-types.h)
     * @param[in] callback     Callback to call when the runlevel switch
     *                         is complete. NULL if no callback is required
     * @param[in] val          Value to pass to the callback
     *
     * @return 0 if the switch command was successful and a non-zero error
     * code otherwise. Note that the return value does not indicate that the
     * runlevel switch occured (the callback will be called when it does) but only
     * that the call to switch runlevel was well formed and will be processed
     * at some point
     */
    u8 (*switchRunlevel)(struct _ocrScheduler_t* self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                         u32 phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*, u64), u64 val);

    // TODO: Check this call
    // u8 (*yield)(struct _ocrScheduler_t *self, ocrGuid_t workerGuid,
    //                    ocrGuid_t yieldingEdtGuid, ocrGuid_t eventToYieldForGuid,
    //                    ocrGuid_t * returnGuid, struct _ocrPolicyCtx_t *context);

    /**
     * @brief Requests EDTs from this scheduler
     *
     * This call requests EDTs from the scheduler. The EDTs are returned in the
     * EDTs array.
     *
     * @param self[in]          Pointer to this scheduler
     * @param count[in/out]     As input contains either:
     *                            - the maximum number of EDTs requested if edts[0] is NULL_GUID
     *                            - the number of EDTs in edts (requested GUIDs). This
     *                              is also the maximum number of EDTs to be returned
     *                          As output, contains the number of EDTs returned
     * @param edts[in/out]      As input contains the GUIDs of the EDTs requested or NULL_GUID.
     *                          As output, contains the EDTs given by the scheduler to the
     *                          caller. Note that the array needs to be allocated by
     *                          the caller and of sufficient size
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*takeEdt)(struct _ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts);

    /**
     * @brief Gives EDTs to this scheduler
     *
     * This call requests that the scheduler now handles the EDTs passed to it. The
     * scheduler may refuse some of the EDTs passed to it
     *
     * @param self[in]          Pointer to this scheduler
     * @param count[in/out]     As input, contains the number of EDTs passed to the scheduler
     *                          As output, contains the number of EDTs still left in the array
     * @param edts[in/out]      As input, contains the EDTs passed to the scheduler. As output,
     *                          contains the EDTs that have not been accepted by the
     *                          scheduler from 0 to count excluded.
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*giveEdt)(struct _ocrScheduler_t *self, u32 *count, ocrFatGuid_t *edts);

    /**
     * @brief Requests communication handler from this scheduler.
     *
     * This call requests communication handler from the scheduler.
     * The pointers are returned in the handlers array.
     *
     * @param self[in]          Pointer to this scheduler
     * @param count[in/out]     Number of handlers. As input, contains the maximum number
     *                          of handlers to be returned. As output contains the number
     *                          of handlers returned.
     * @param handlers[in/out]  As output, handlers given to the caller by the callee where
     *                          the fatGuid's metaDataPtr contains a pointer to a handler.
     *                          The fatGuid's guid is set to NULL_GUID.
     *                          (array needs to be allocated by the caller and of sufficient size).
     * @param properties[in]    Properties for the take
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*takeComm)(struct _ocrScheduler_t *self, u32 *count, ocrFatGuid_t * handlers, u32 properties);

    /**
     * @brief Gives communication handler to this scheduler.
     *
     * @param self[in]          Pointer to this scheduler
     * @param count[in/out]     As input, contains the number of handlers passed to the scheduler
     *                          As output, contains the number of handlers still left in the array
     * @param handlers[in/out]  As input, contains handlers passed to the scheduler through the fatGuid's
     *                          metaDataPtr. The fatGuid's guid is set to NULL_GUID. As output,
     *                          contains handlers that have not been accepted by the scheduler
     *                          from 0 to count excluded.
     * @param properties[in]    Properties for the give
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*giveComm)(struct _ocrScheduler_t *self, u32 *count, ocrFatGuid_t * handlers, u32 properties);

    /**
     * @brief Ask the scheduler to monitor the progress of an operation
     * TODO needs more work.
     * @param self[in]          Pointer to this scheduler
     * @param type[in]          The type of the operation
     * @param monitoree[in]     The data-structure associated with the operation
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*monitorProgress)(struct _ocrScheduler_t *self, ocrMonitorProgress_t type, void * monitoree);

    ////////////////////////////////////////////////////////////////////////////////////////////////////
    // Scheduler 1.0 //
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * @brief Register the contextId with the scheduler
     *
     * The scheduler keeps track of all agents that
     * it communicate with as contexts. It registers those
     * contexts with each scheduler heuristic.
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param contextId[in]      Context ID known to policy domain
     * @param loc[in]            Absolute location for this context ID
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*registerContext)(struct _ocrScheduler_t *self, u64 contextId, ocrLocation_t loc);

    /**
     * @brief Scheduler updates itself
     *
     * This works as a proactive monitoring hook
     *
     * @param self[in]          Pointer to this scheduler
     * @param opArgs[in]        Info about scheduler update
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*update)(struct _ocrScheduler_t *self, ocrSchedulerOpArgs_t *opArgs);

    //Array of functions for scheduler ops
    ocrSchedulerOpFcts_t op[NUM_SCHEDULER_OPS];
} ocrSchedulerFcts_t;

struct _ocrWorkpile_t;

/*! \brief Represents OCR schedulers.
 *
 *  Currently, we allow scheduler interface to have work taken from them or given to them
 */
typedef struct _ocrScheduler_t {
    ocrFatGuid_t fguid;
    struct _ocrPolicyDomain_t *pd;
    u64 contextCount;                           /**< The number of contexts handled by the scheduler */

    struct _ocrWorkpile_t **workpiles;
    u64 workpileCount;

    // SchedulerObject Root:
    // Top level schedulerObject that encapsulates all schedulerObjects.
    // SchedulerObject factories are maintained in the policy domain.
    struct _ocrSchedulerObjectRoot_t *rootObj;

    // Scheduler Heuristics
    struct _ocrSchedulerHeuristic_t **schedulerHeuristics;
    u64 schedulerHeuristicCount;

    ocrSchedulerFcts_t fcts;
} ocrScheduler_t;

/****************************************************/
/* OCR SCHEDULER FACTORY                            */
/****************************************************/

typedef struct _ocrSchedulerFactory_t {
    ocrScheduler_t* (*instantiate) (struct _ocrSchedulerFactory_t * factory,
                                    ocrParamList_t *perInstance);
    void (*initialize) (struct _ocrSchedulerFactory_t * factory, ocrScheduler_t *self, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrSchedulerFactory_t * factory);

    ocrSchedulerFcts_t schedulerFcts;
} ocrSchedulerFactory_t;

void initializeSchedulerOcr(ocrSchedulerFactory_t * factory, ocrScheduler_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_SCHEDULER_H__ */
