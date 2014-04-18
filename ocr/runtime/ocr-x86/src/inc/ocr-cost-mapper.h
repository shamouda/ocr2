/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_COST_MAPPER_H__
#define __OCR_COST_MAPPER_H__

#include "ocr-runtime-types.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

struct _ocrPolicyDomain_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListCostMapperFact_t {
    ocrParamList_t base;
} paramListCostMapperFact_t;

typedef struct _paramListCostMapperInst_t {
    ocrParamList_t base;
} paramListCostMapperInst_t;


/****************************************************/
/* OCR COST_MAPPER                                  */
/****************************************************/

struct _ocrCostMapper_t;
struct _ocrCost_t;
struct _ocrPolicyCtx_t;
struct _ocrMsgHandler_t;

typedef enum {
    OCR_SCHED_COST_TYPE_INTRINSIC,
    OCR_SCHED_COST_TYPE_SETUP,
    OCR_SCHED_COST_TYPE_DELTA_SYSTEM,
    OCR_SCHED_COST_TYPE_MAX
} ocrCostType_t;

typedef struct _ocrSchedCost_t {
    double cost[OCR_SCHED_COST_TYPE_MAX];
} ocrSchedCost_t;

/* Provides delta of new mapping to current system mapping */
typedef struct _ocrSchedMapping_t {
    ocrFatGuid_t *elements;
    ocrLocation_t *locations;     /* TODO: Mem locations? */
} ocrSchedMapping_t;

typedef struct _ocrCostMapperFcts_t {
    void (*destruct)(struct _ocrCostMapper_t *self);

    void (*begin)(struct _ocrCostMapper_t *self, struct _ocrPolicyDomain_t * PD);

    void (*start)(struct _ocrCostMapper_t *self, struct _ocrPolicyDomain_t * PD);

    void (*stop)(struct _ocrCostMapper_t *self);

    void (*finish)(struct _ocrCostMapper_t *self);

    /**
     * @brief Takes potential components from this cost mapper
     *
     * This call requests components from the cost mapper.
     * They are returned in the components array.
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param source[in]         Location that is asking for components
     * @param count[in/out]      As input contains the maximum number of components requested.
     *                           As output, contains the actual number of components returned
     * @param components[in/out] As input contains the GUIDs of the components requested or NULL_GUID.
     *                           As output, contains the components given by the scheduler to the
     *                           caller. Note that the array needs to be allocated by
     *                           the caller and of sufficient size
     *                           For DB create, the input GUID will contain a partial metadata without allocation.
     * @param hints[in]          Hints for the take.
     * @param properties[in]     Properties array for the take based on scheduler properties
     * @param costs[out]         Array of costs generated for components
     * @param mapping[out]       Array of location mappings generated for each element in every component
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*take)(struct _ocrCostMapper_t *self, ocrLocation_t source, u32 *count, ocrFatGuid_t *components, ocrFatGuid_t *hints, ocrSchedulerProp_t *properties, ocrSchedCost_t *costs, ocrSchedMapping_t *mappings);

    /**
     * @brief Gives components to this cost mapper
     *
     * The Components are given in the components array.
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param source[in]         Location that is asking for components
     * @param count[in/out]      As input contains the number of components passed to the scheduler.
     *                           As output, contains the number of components remaining in the array.
     * @param components[in/out] As input contains the GUIDs of the components given to the cost mapper.
     *                           As output, contains the GUIDs of components remaining in the array.
     * @param hints[in]          Hints for the give.
     * @param properties[in]     Properties array for the give based on scheduler properties
     * @param costs[out]         Array of costs generated for components
     * @param mapping[out]       Array of location mappings generated for each element in every component
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*give)(struct _ocrCostMapper_t *self, ocrLocation_t source, u32 *count, ocrFatGuid_t *components, ocrFatGuid_t *hints, ocrSchedulerProp_t *properties);

    /**
     * @brief Update this cost mapper after scheduler decision
     *
     * The Components are given in the components array.
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param count[in]          Contains the number of components that have been updated.
     * @param components[in]     Contains the GUIDs of the components that have been updated.
     * @param mapping[out]       Array of location mappings for each element in every component
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*update)(struct _ocrCostMapper_t *self, u32 *count, ocrFatGuid_t *components, ocrSchedMapping_t *mappings);
} ocrCostMapperFcts_t;

struct _ocrWorkpile_t;

/*! \brief Represents OCR schedulers.
 *
 *  Currently, we allow scheduler interface to have work taken from them or given to them
 */
typedef struct _ocrCostMapper_t {
    ocrFatGuid_t fguid;
    struct _ocrPolicyDomain_t *pd;

    struct _ocrComponent_t *componentState; /**< Component that holds the state of all components under the jurisdiction of this cost mapper */

    ocrCostMapperFcts_t fcts;
} ocrCostMapper_t;


/****************************************************/
/* OCR COST_MAPPER FACTORY                          */
/****************************************************/

typedef struct _ocrCostMapperFactory_t {
    ocrCostMapper_t* (*instantiate) (struct _ocrCostMapperFactory_t * factory,
                                    ocrParamList_t *perInstance);
    void (*initialize) (struct _ocrCostMapperFactory_t * factory, ocrCostMapper_t *self, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrCostMapperFactory_t * factory);

    ocrCostMapperFcts_t costMapperFcts;
} ocrCostMapperFactory_t;

void initializeCostMapperOcr(ocrCostMapperFactory_t * factory, ocrCostMapper_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_COST_MAPPER_H__ */
