/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_SCHEDULER_HEURISTIC_H__
#define __OCR_SCHEDULER_HEURISTIC_H__

#include "ocr-runtime-types.h"
#include "ocr-runtime-hints.h"
#include "ocr-types.h"
#include "ocr-scheduler.h"
#include "ocr-scheduler-object.h"
#include "utils/ocr-utils.h"

struct _ocrScheduler_t;
struct _ocrSchedulerOpArgs_t;
struct _ocrSchedulerObject_t;
struct _ocrSchedulerHeuristic_t;
struct _ocrSchedulerHeuristicContext_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListSchedulerHeuristicFact_t {
    ocrParamList_t base;
} paramListSchedulerHeuristicFact_t;

typedef struct _paramListSchedulerHeuristic_t {
    ocrParamList_t base;
} paramListSchedulerHeuristic_t;

/****************************************************/
/* OCR COST METRICS                                 */
/****************************************************/

typedef enum {
    OCR_SCHED_COST_TYPE_INTRINSIC,
    OCR_SCHED_COST_TYPE_SETUP,
    OCR_SCHED_COST_TYPE_DELTA_SYSTEM,
    OCR_SCHED_COST_TYPE_MAX
} ocrCostType_t;

typedef struct _ocrCost_t {
    //TBD
} ocrCost_t;

typedef struct _ocrCostTable_t {
    //TBD
} ocrCostTable_t;

/****************************************************/
/* OCR SCHEDULER HEURISTIC OPERATIONS               */
/****************************************************/

typedef enum {
    OCR_SCHEDULER_HEURISTIC_OP_GIVE,
    OCR_SCHEDULER_HEURISTIC_OP_TAKE,
    NUM_SCHEDULER_HEURISTIC_OPS,
} ocrSchedulerHeuristicOp_t;

/****************************************************/
/* OCR SCHEDULER HEURISTIC CONTEXT                  */
/****************************************************/

/*! \brief The scheduler heuristic context is used to record
 *   actions that can be performed by simply replaying
 *   them, and also to cache information from past operation
 *   that can be useful in future decision making.
 *   The scheduler heuristic maintains one context per client, i.e,
 *   locations that will communicate with this scheduler.
 */
typedef struct _ocrSchedulerHeuristicContext_t {
    u64 id;                             /**< SchedulerObject Context Id
                                             ID that is unique to a location (see registerNeighbor() in ocr-scheduler.h)
                                             In other words, for a single location, all scheduler-heuristics will index that location
                                             with the same ID */
    ocrSchedulerObjectActionSet_t *actionSet; /**< Actions returned by the scheduler heuristic */
    ocrCost_t *cost;                    /**< TBD: (Placeholder) Cost values returned by the scheduler heuristic to the scheduler */
    u32 properties;                     /**< Properties for the schedulerObject context */
} ocrSchedulerHeuristicContext_t;

/****************************************************/
/* OCR SCHEDULER_HEURISTIC                          */
/****************************************************/

typedef struct _ocrSchedulerHeuristicOpFcts_t {
    /**
     * @brief Invokes the scheduler heuristic for a real mode operation.
     *
     * If the context's actionSet is not null, then the scheduler heuristic
     * plays out the recorded set of actions. Otherwise,
     * the scheduler heuristic immediately acts out all the actions to
     * be performed. In the former case, the context is updated
     * to remove all actions performed. In both cases, the context
     * is updated with whatever information is required to be cached
     * for the next invokation.
     *
     * Called by the scheduler.
     * Based on operation, opArgs->schedulerObject contains either
     * the element or the allocation that will hold the element.
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param context[in/out]    Context for the operation
     * @param opArgs[in]         Info about specific scheduling task
     * @param hints[in]          Hints for the operation
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*invoke)(struct _ocrSchedulerHeuristic_t *self, struct _ocrSchedulerHeuristicContext_t *context, struct _ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints);

    /**
     * @brief Requests the scheduler heuristic to simulate a real operation
     *
     * The scheduler heuristic records the simulated set of actions in the
     * context. The context also holds the potential cost of performing
     * these actions. After the simulation call returns, the scheduler
     * can read the context (based on contextId), and select a specific
     * action set.
     *
     * Called by the scheduler.
     * Based on operation, opArgs->schedulerObject contains either
     * the element (for a 'give' for example)
     * or the allocation that will hold the element (for a 'take'
     * for example)
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param context[in/out]    Context for the simulation
     * @param opArgs[in]         Info about specific scheduling task
     * @param hints[in]          Hints for the operation
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*simulate)(struct _ocrSchedulerHeuristic_t *self, struct _ocrSchedulerHeuristicContext_t *context, struct _ocrSchedulerOpArgs_t *opArgs, ocrRuntimeHint_t *hints);

    /**
     * @brief Rejects a simulation result to proceed towards invoke
     *
     * After a simulation is complete, the results are analyzed
     * by the scheduler driver heuristic. If those results were
     * found sub-optimal compared to another one, then these results
     * are rejected. The scheduler heuristic that created these results are
     * notified of the winner so that it may learn from defeat.
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param context[in/out]    Context for the rejected simulation
     * @param winner[in]         Winner of the simulation operation.
     *                           Contains the actionSet and costs
     *                           for the winner.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*reject)(struct _ocrSchedulerHeuristic_t *self, struct _ocrSchedulerHeuristicContext_t *context, struct _ocrSchedulerHeuristicContext_t *winner);

} ocrSchedulerHeuristicOpFcts_t;

typedef struct _ocrSchedulerHeuristicFcts_t {
    void (*destruct)(struct _ocrSchedulerHeuristic_t *self);

    void (*begin)(struct _ocrSchedulerHeuristic_t *self);

    void (*start)(struct _ocrSchedulerHeuristic_t *self);

    void (*stop)(struct _ocrSchedulerHeuristic_t *self);

    void (*finish)(struct _ocrSchedulerHeuristic_t *self);

    /**
     * @brief Update this scheduler heuristic proactively
     *
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param properties[in]     Properties of this update
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*update)(struct _ocrSchedulerHeuristic_t *self, u32 properties);

    /**
     * @brief Get the scheduler heuristic context for a specific contextId
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param contextId[in]      Context ID known to the scheduler
     *
     * @return 0 on success and a non-zero value on failure
     */
    ocrSchedulerHeuristicContext_t* (*getContext)(struct _ocrSchedulerHeuristic_t *self, u64 contextId);

    /**
     * @brief Register the contextId with the scheduler heuristic
     *
     * This lets the scheduler heuristic be aware of a contextId
     * and so it initializes the scheduler heuristic context for
     * the contextId.
     *
     * @param self[in]           Pointer to this scheduler heuristic
     * @param contextId[in]      Context ID known to policy domain
     * @param loc[in]            Absolute location for this context ID
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*registerContext)(struct _ocrSchedulerHeuristic_t *self, u64 contextId, ocrLocation_t loc);

    ocrSchedulerHeuristicOpFcts_t op[NUM_SCHEDULER_HEURISTIC_OPS];  /*< Array of invoke/simulate functions based on operation type */
} ocrSchedulerHeuristicFcts_t;

/*! \brief Represents OCR scheduler heuristics.
 *
 *  Scheduler heuristics are meant to implement the scheduling heuristics.
 *  Each scheduler heuristic is meant to optimize for a specific objective function.
 *  E.g: an objective function that optimzes energy usage may have scheduling
 *  decisions separate to one which optimizes time or space.
 *
 *  Scheduler heuristics are meant to be thread-safe so they can be invoked concurrently
 *  by multiple scheduling instances.
 */
typedef struct _ocrSchedulerHeuristic_t {
    ocrFatGuid_t fguid;
    struct _ocrScheduler_t *scheduler;              /*< The scheduler which owns this scheduler heuristic */
    struct _ocrSchedulerHeuristicContext_t **contexts;      /*< All the contexts maintained by this scheduler heuristic */
    u64 contextCount;                               /*< Total number of contexts maintained by this scheduler heuristic */
    ocrCostTable_t *costTable;                      /*< TBD: Placeholder for a cost table */
    ocrSchedulerHeuristicFcts_t fcts;                       /*< Functions called by the scheduler */
} ocrSchedulerHeuristic_t;

/****************************************************/
/* OCR SCHEDULER_HEURISTIC FACTORY                  */
/****************************************************/

typedef struct _ocrSchedulerHeuristicFactory_t {
    ocrSchedulerHeuristic_t* (*instantiate) (struct _ocrSchedulerHeuristicFactory_t * factory, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrSchedulerHeuristicFactory_t * factory);
    ocrSchedulerHeuristicFcts_t fcts;
} ocrSchedulerHeuristicFactory_t;

void initializeSchedulerHeuristicOcr(ocrSchedulerHeuristicFactory_t * factory, ocrSchedulerHeuristic_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_SCHEDULER_HEURISTIC_H__ */