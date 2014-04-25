/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_SCHEDULER_H__
#define __OCR_SCHEDULER_H__

#include "ocr-runtime-types.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

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
/* OCR SCHEDULER                                    */
/****************************************************/

struct _ocrScheduler_t;
struct _ocrCost_t;
struct _ocrPolicyCtx_t;
struct _ocrMsgHandler_t;

/* Scheduler Properties */
#define SCHED_PROP_READY 0x0
#define SCHED_PROP_NEW   0x1
#define SCHED_PROP_DONE  0x2

typedef struct _ocrSchedulerFcts_t {
    void (*destruct)(struct _ocrScheduler_t *self);

    void (*begin)(struct _ocrScheduler_t *self, struct _ocrPolicyDomain_t * PD);

    void (*start)(struct _ocrScheduler_t *self, struct _ocrPolicyDomain_t * PD);

    void (*stop)(struct _ocrScheduler_t *self);

    void (*finish)(struct _ocrScheduler_t *self);

    /**
     * @brief Takes a component from this scheduler
     *
     * This call requests a component from the scheduler.
     * The clients of this call could be a worker or another scheduler.
     *
     * @param self[in]           Pointer to this scheduler
     * @param source[in]         Location that is asking for components
     * @param component[in/out]  As input contains the GUID of the component requested or NULL_GUID.
     *                           As output, contains the component given by the scheduler to the caller.
     *                           For DB create, the input GUID will contain a partial metadata without allocation.
     * @param hints[in]          Hints for the take.
     *                           E.g: A scheduler can hint to another scheduler which cost mapper to use.
     * @param properties[in]     Properties for the take
     *                           This call can be used with the following properties:
     *                           1> SCHED_PROP_READY:
     *                              In this mode, the scheduler will return a component with ready elements.
     *                              If no component input is provided, then the scheduler returns the best component.
     *                              If a component input GUID is provided, then the scheduler returns that component
     *                              if it is readily available. An error is thrown if not ready.
     *                              If an input GUID is a DB, then the scheduler acquires it for the requester.
     *                              An error is thrown if the DB was not acquired.
     *                           2> SCHED_PROP_NEW:
     *                              In this mode, the scheduler will allocate new components.
     *                              The component input cannot contain a NULL_GUID.
     *                              The input GUID should point to the component metadata.
     *                              Information regarding allocation size and hints etc, will be taken from the input GUID metadata.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*take)(struct _ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties);

    /**
     * @brief Gives a component to this scheduler
     *
     * This call gives a component to the scheduler.
     * The clients of this call could be a worker or another scheduler.
     *
     * @param self[in]           Pointer to this scheduler
     * @param source[in]         Location that is giving the component
     * @param component[in]      Contains the GUID of the components given to the scheduler.
     * @param hints[in]          Hints for the give.
     *                           E.g: A client can hint to the schduler which component to associate this entry with.
     *                           Or, a scheduler can be hinted the cost update for a done component.
     * @param properties[in]     Properties for the give
     *                           This call can be used with the following properties:
     *                           1> SCHED_PROP_READY:
     *                              In this mode, the scheduler is handed a ready component.
     *                              There could be ready EDTs/DBs/Comms etc.
     *                           2> SCHED_PROP_DONE:
     *                              In this mode, the scheduler is notified that the component has completed execution.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*give)(struct _ocrScheduler_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties);

    /**
     * @brief Provides a cost update for a component
     *
     * A scheduler element could be an EDT or DB or Comm.
     * The clients of this call could be the statistics/introspection module, or
     * it could be used as the backend of user-level APIs.
     *
     * @param self[in]          Pointer to this scheduler
     * @param component[in]     Component whose cost is set
     * @param cost[in]          Cost to set
     * @param properties[in]    Properties of the cost
     */
    u8 (*setCost)(struct _ocrScheduler_t *self, ocrFatGuid_t component, ocrFatGuid_t cost, u32 properties);

    /**
     * @brief Provides a cost update for a pair of components relative to each other
     *
     * The component could be an EDT or DB or Comm.
     *
     * @param self[in]          Pointer to this scheduler
     * @param componentSrc[in]  Component that is the source of this relation (E.g: for directional affinities)
     * @param componentDst[in]  Component that is the destination of this relation
     * @param relativeCost[in]  Relative cost
     * @param properties[in]    Properties of the cost
     */
    u8 (*setRelativeCost)(struct _ocrScheduler_t *self, ocrFatGuid_t componentSrc, ocrFatGuid_t componentDst, ocrFatGuid_t relativeCost, u32 properties);

    /**
     * @brief Scheduler updates itself
     *
     * This works as a proactive monitoring hook
     *
     * @param self[in]          Pointer to this scheduler
     * @param properties[in]    Properties of this update
     */
    void (*update)(struct _ocrScheduler_t *self, u32 properties);
} ocrSchedulerFcts_t;

/*! \brief Represents OCR schedulers.
 *
 *  Currently, we allow scheduler interface to have work taken from them or given to them
 */
typedef struct _ocrScheduler_t {
    ocrFatGuid_t fguid;
    struct _ocrPolicyDomain_t *pd;

    struct _ocrCostMapper_t **mappers;
    u32 mapperCount;

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
