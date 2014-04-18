/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_COMPONENT_H_
#define __OCR_COMPONENT_H_

#include "ocr-runtime-types.h"
#include "ocr-tuning.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

struct _ocrPolicyDomain_t;


/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListComponentFact_t {
    ocrParamList_t base;
} paramListComponentFact_t;

typedef struct _paramListComponentInst_t {
    ocrParamList_t base;
} paramListComponentInst_t;


/****************************************************/
/* OCR COMPONENT                                     */
/****************************************************/

struct _ocrComponent_t;
struct _ocrPolicyDomain_t;

typedef struct _ocrComponentFcts_t {
    void (*destruct)(struct _ocrComponent_t *self);

    void (*begin)(struct _ocrComponent_t *self, struct _ocrPolicyDomain_t *PD);

    void (*start)(struct _ocrComponent_t *self, struct _ocrPolicyDomain_t *PD);

    void (*stop)(struct _ocrComponent_t *self);

    void (*finish)(struct _ocrComponent_t *self);

    /** @brief Create new elements inside this component
     *
     *  This will create new element components and register them with this component set.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] count        Number of components to create
     *  @param[out] components  Array of new components
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*create)(struct _ocrComponent_t *self, u32 count, ocrFatGuid_t *components);

    /** @brief Insert elements into this component
     *
     *  This will insert and register element components with this component set.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] count        Number of components to insert
     *  @param[in] components   Array of components to insert
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*insert)(struct _ocrComponent_t *self, u32 count, ocrFatGuid_t *components);

    /** @brief Removes elements from this component
     *
     *  This will remove and de-register element components from this component set.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] count        Number of components to remove
     *  @param[in] components   Array of components to remove
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*remove)(struct _ocrComponent_t *self, u32 count, ocrFatGuid_t *components);

    /** @brief Moves elements from this component
     *
     *  This will move and de-register element components from this component set.
     *
     *  @param[in] self         Pointer to this (source) component
     *  @param[in] destination  Pointer to destination component
     *  @param[in] count        Number of components to move
     *  @param[in] components   Array of components to move
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*move)(struct _ocrComponent_t *self, struct _ocrComponent_t *destination, u32 count, ocrFatGuid_t *components);

    /** @brief Splits this component
     *
     *  This will split this component into multiple parts.
     *  The current component will be de-registered from the superset component.
     *  The newly created components will be registered with the superset component.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] count        Number of components to create from split
     *  @param[in] offsets      Array of offsets in the element array as the start of each part
     *  @param[out] components  Array of components created from the split
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*split)(struct _ocrComponent_t *self, u32 count, u32 *offsets, ocrFatGuid_t *components);

    /** @brief Merges multiple components into this component
     *
     *  This will merge multiple parts into this component.
     *  All components other than this component will be de-registered from the superset component.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] count        Number of components to be merged into this component
     *  @param[in] components   Array of components to be merged into this component
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*merge)(struct _ocrComponent_t *self, u32 count, ocrFatGuid_t *components);

    /* Get the number of elements */
    u32 getElementCount();
} ocrComponentFcts_t;

/*! \brief Abstract class to represent OCR component data structures.
 *
 */
typedef struct _ocrComponent_t {
    ocrFatGuid_t fguid;
    struct _ocrPolicyDomain_t *pd;
    ocrComponentFcts_t fcts;
} ocrComponent_t;


/****************************************************/
/* OCR COMPONENT FACTORY                            */
/****************************************************/

typedef struct _ocrComponentFactory_t {
    ocrComponent_t * (*instantiate)(struct _ocrComponentFactory_t * factory, ocrParamList_t *perInstance);
    void (*initialize) (struct _ocrComponentFactory_t * factory, struct _ocrComponent_t * worker, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrComponentFactory_t * factory);
    ocrComponentFcts_t componentFcts;
} ocrComponentFactory_t;

void initializeComponentOcr(ocrComponentFactory_t * factory, ocrComponent_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_COMPONENT_H_ */
