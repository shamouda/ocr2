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

typedef struct _ocrCostMapperFcts_t {
    void (*destruct)(struct _ocrCostMapper_t *self);

    void (*begin)(struct _ocrCostMapper_t *self, struct _ocrPolicyDomain_t * PD);

    void (*start)(struct _ocrCostMapper_t *self, struct _ocrPolicyDomain_t * PD);

    void (*stop)(struct _ocrCostMapper_t *self);

    void (*finish)(struct _ocrCostMapper_t *self);

    /**
     * @brief Takes a potential component from this cost mapper
     *
     * This call requests components from the cost mapper.
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param source[in]         Location that is asking for components
     * @param component[in/out]  As input contains the GUID of the components requested or NULL_GUID.
     *                           As output, contains the GUID of component returned by the mapper.
     * @param hints[in]          Hints for the take.
     * @param costs[out]         Costs generated for component
     * @param mapping[out]       Location mappings generated for each element in the component
     * @param properties[in]     Properties for the take
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*take)(struct _ocrCostMapper_t *self, ocrLocation_t source, ocrFatGuid_t *component, ocrFatGuid_t hints, ocrFatGuid_t *costs, ocrFatGuid_t *mappings, u32 properties);

    /**
     * @brief Gives a components to this cost mapper
     *
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param source[in]         Location that is giving the component
     * @param component[in]      Contains the GUID of the component given to the cost mapper.
     * @param hints[in]          Hints for the give.
     * @param properties[in]     Properties for the give
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*give)(struct _ocrCostMapper_t *self, ocrLocation_t source, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties);

    /**
     * @brief Update this cost mapper after scheduler decision
     *
     * The Components are given in the components array.
     * The client of this call is the scheduler driver heuristic.
     *
     * @param self[in]           Pointer to this cost mapper
     * @param component[in]      Contains the GUID of the component that has been selected.
     * @param mapping[out]       Location mappings for each element in the component
     * @param properties[in]     Properties of this update
     *
     * @return 0 on success and a non-zero value on failure
     */
    u8 (*update)(struct _ocrCostMapper_t *self, ocrFatGuid_t component, ocrFatGuid_t mappings, u32 properties);
} ocrCostMapperFcts_t;

/*! \brief Represents OCR cost mappers.
 *
 *  Currently, we allow scheduler interface to have work taken from them or given to them
 */
typedef struct _ocrCostMapper_t {
    ocrFatGuid_t fguid;
    struct _ocrComponent_t *componentState; /**< Component that holds the state of all components under the jurisdiction of this cost mapper */
    ocrCostMapperFcts_t fcts;
} ocrCostMapper_t;


/****************************************************/
/* OCR COST_MAPPER FACTORY                          */
/****************************************************/

typedef struct _ocrCostMapperFactory_t {
    ocrCostMapper_t* (*instantiate) (struct _ocrCostMapperFactory_t * factory,
                                    ocrParamList_t *perInstance);
    //void (*initialize) (struct _ocrCostMapperFactory_t * factory, ocrCostMapper_t *self, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrCostMapperFactory_t * factory);

    ocrCostMapperFcts_t fcts;
} ocrCostMapperFactory_t;

#endif /* __OCR_COST_MAPPER_H__ */
