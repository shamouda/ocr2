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
typedef enum {
    SCHED_PROP_READY = 0x0,
    SCHED_PROP_NEW   = 0x1,
    SCHED_PROP_DONE  = 0x2
} ocrSchedulerProp_t;

typedef struct _ocrSchedulerFcts_t {
    void (*destruct)(struct _ocrScheduler_t *self);

    void (*begin)(struct _ocrScheduler_t *self, struct _ocrPolicyDomain_t * PD);

    void (*start)(struct _ocrScheduler_t *self, struct _ocrPolicyDomain_t * PD);

    void (*stop)(struct _ocrScheduler_t *self);

    void (*finish)(struct _ocrScheduler_t *self);

    /**
     * @brief Takes components from this scheduler
     *
     * This call requests components from the scheduler.
     * They are returned in the components array.
     * The clients of this call could be a worker or another scheduler.
     *
     * @param self[in]           Pointer to this scheduler
     * @param source[in]         Location that is asking for edts
     * @param count[in/out]      As input contains the maximum number of components requested.
     *                           As output, contains the actual number of components returned
     * @param components[in/out] As input contains the GUIDs of the components requested or NULL_GUID.
     *                           As output, contains the components given by the scheduler to the
     *                           caller. Note that the array needs to be allocated by
     *                           the caller and of sufficient size
     *                           For DB create, the input GUID will contain a partial metadata without allocation.
     * @param hints[in]          Hints for the take.
     *                           E.g: A scheduler can hint to another scheduler which cost mapper to use.
     * @param properties[in]     Properties array for the take
     *                           This call can be used with the following properties:
     *                           1> SCHED_PROP_READY:
     *                              In this mode, the scheduler will return ready components.
     *                              If the components array has NULL_GUIDs, then the scheduler returns best of available components.
     *                              If the components array has specific GUIDs, then the scheduler those components.
     *                              If an input GUID is a DB, then the scheduler acquires it for the requester.
     *                           2> SCHED_PROP_NEW:
     *                              In this mode, the scheduler will allocate new components.
     *                              The components array cannot contain a NULL_GUID.
     *                              The input GUIDs should point to the component metadata.
     *                              Information regarding allocation size, hints etc, will be taken from the input GUID metadata.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*take)(struct _ocrScheduler_t *self, ocrLocation_t source, u32 *count, ocrFatGuid_t *components, ocrFatGuid_t *hints, ocrSchedulerProp_t *properties);

    /**
     * @brief Gives Components to this scheduler
     *
     * This call gives Components to the scheduler.
     * The Components are given in the components array.
     * The clients of this call could be a worker or another scheduler.
     *
     * @param self[in]           Pointer to this scheduler
     * @param source[in]         Location that is asking for edts
     * @param count[in/out]      As input contains the number of components passed to the scheduler.
     *                           As output, contains the number of components remaining in the array.
     * @param components[in/out] As input contains the GUIDs of the components given to the scheduler.
     *                           As output, contains the GUIDs of components remaining in the array.
     * @param hints[in]          Hints for the give.
     *                           E.g: A client can hint to the schduler which component to associate this entry with.
     *                           Or, a scheduler can be hinted the cost update for a done component.
     * @param properties[in]     Properties array for the give
     *                           This call can be used with the following properties:
     *                           1> SCHED_PROP_READY:
     *                              In this mode, the scheduler is handed ready components.
     *                              There could be ready EDTs/DBs/Comms etc.
     *                           2> SCHED_PROP_DONE:
     *                              In this mode, the scheduler is notified that components have completed execution.
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*give)(struct _ocrScheduler_t *self, ocrLocation_t source, u32 *count, ocrFatGuid_t *components, ocrFatGuid_t *hints, ocrSchedulerProp_t *properties);

    /**
     * @brief Provides a cost update for a scheduler element
     *
     * A scheduler element could be an EDT or DB or Comm.
     * The clients of this call could be the statistics/introspection module, or
     * it could be used as the backend of user-level APIs.
     *
     * @param self[in]          Pointer to this scheduler
     * @param count[in]         The number of elements for which to set cost
     * @param elements[in]      Array of elements passed to the scheduler
     * @param elementCosts[in]  Array of costs for each elements passed to the scheduler
     */
    u8 (*setCost)(struct _ocrScheduler_t *self, u32 count, ocrFatGuid_t *elements, ocrFatGuid_t *elementCosts);

    /**
     * @brief Provides a cost update for a pair of scheduler elements relative to each other
     *
     * A scheduler element could be an EDT or DB or Comm.
     *
     * @param self[in]          Pointer to this scheduler
     * @param paircount[in]     The number of pairs for which to set cost
     * @param pairElements[in]  Array of element pairs passed to the scheduler
     *                          Each pair is packed into adjacent array indexes
     *                          (length of array = 2 * paircount)
     * @param relativeCosts[in] Array of costs for each pair passed to the scheduler
     */
    u8 (*setRelativeCost)(struct _ocrScheduler_t *self, u32 pairCount, ocrFatGuid_t *pairElements, ocrFatGuid_t *relativeCosts);

    /**
     * @brief Scheduler updates itself
     *
     * This works as a proactive monitoring hook
     *
     * @param self[in]          Pointer to this scheduler
     */
    void (*update)(struct _ocrScheduler_t *self);
} ocrSchedulerFcts_t;

struct _ocrWorkpile_t;

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
