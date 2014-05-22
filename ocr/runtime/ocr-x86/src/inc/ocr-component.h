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
/* OCR COMPONENT TYPES                              */
/****************************************************/
#define OCR_COMP_PROP_TYPE      0xFFFF
#define OCR_COMP_PROP_TYPE_EDT  0x1
#define OCR_COMP_PROP_TYPE_DB   0x2
/*< 12 bits reserved for implementation specific types: Prop type 0x10 to 0x8000 >*/

/****************************************************/
/* OCR COMPONENT                                    */
/****************************************************/

struct _ocrComponent_t;
struct _ocrPolicyDomain_t;

typedef struct _ocrComponentFcts_t {

    /** @brief Create a new element inside this component
     *
     *  This will create new element components and register them with this component set.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location that wants to create
     *  @param[out] component   new component
     *  @param[in] hints        Hints for the create.
     *  @param[in] properties   Properties for the create.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*create)(struct _ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t* component, ocrFatGuid_t hints, u32 properties);

    /** @brief Insert an element into this component
     *
     *  This will insert and register element components with this component set.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location that wants to insert
     *  @param[in] component    component to insert
     *  @param[in] hints        Hints for the insert.
     *  @param[in] properties   Properties for the insert.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*insert)(struct _ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties);

    /** @brief Removes an element from this component
     *
     *  This will remove and de-register element components from this component set.
     *
     *  @param[in] self          Pointer to this component
     *  @param[in] loc           Location that wants to remove
     *  @param[in/out] component Component to remove. Cannot be null pointer.
     *                           If component->guid is not NULL_GUID, remove that specific component.
     *                           If component->guid is NULL_GUID, remove and return any component (implementation specific)
     *  @param[in] hints         Hints for the remove.
     *  @param[in] properties    Properties for the remove.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*remove)(struct _ocrComponent_t *self, ocrLocation_t loc, ocrFatGuid_t *component, ocrFatGuid_t hints, u32 properties);

    /** @brief Moves an element from this component to another component
     *
     *  This will move and de-register element components from this component set.
     *
     *  @param[in] self         Pointer to this (source) component
     *  @param[in] loc          Location that wants to move
     *  @param[in] remote       Pointer to remote component
     *  @param[in] component    Component to move
     *  @param[in] hints        Hints for the move.
     *  @param[in] properties   Properties for the move.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*move)(struct _ocrComponent_t *self, ocrLocation_t loc, struct _ocrComponent_t *destination, ocrFatGuid_t component, ocrFatGuid_t hints, u32 properties);

    /** @brief Splits this component
     *
     *  This will split this component into multiple parts.
     *  The current component will be de-registered from the superset component.
     *  The newly created components will be registered with the parent component.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location that wants to split
     *  @param[in] count        Number of components to create from split
     *  @param[in] chunks       Array of chunk sizes (Sum of chunk sizes = numElements)
     *  @param[out] components  Array of components created from the split
     *  @param[in] hints        Hints for the split.
     *  @param[in] properties   Properties for the split.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*split)(struct _ocrComponent_t *self, ocrLocation_t loc, u32 count, u32 *chunks, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties);

    /** @brief Merges multiple components into this component
     *
     *  This will merge multiple parts into this component.
     *  All components other than this component will be de-registered from the superset component.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location that wants to merge
     *  @param[in] count        Number of components to be merged into this component
     *  @param[in] components   Array of components to be merged into this component
     *  @param[in] hints        Hints for the merge.
     *  @param[in] properties   Properties for the merge.
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*merge)(struct _ocrComponent_t *self, ocrLocation_t loc, u32 count, ocrFatGuid_t *components, ocrFatGuid_t hints, u32 properties);

    /** @brief Get the number of elements
     *
     *  This will return the number of elements in the component
     *  The implementation may expose this as a [0,infinity] function.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location that wants to know the count
     *  @return Returns the number of components or a property
     *  If return value is: OCR_COMPONENT_SIZE_NON_EMPTY,
     *  it simply means that more than one component exists
     */
    u64 (*count)(struct _ocrComponent_t *self, ocrLocation_t loc);

    /** @brief Set the location for this component
     *
     *  This will set the component's location to which it is mapped.
     *
     *  @param[in] self         Pointer to this component
     *  @param[in] loc          Location mapping
     *
     *  @return 0 on success and a non-zero value on failure
     */
    u8 (*setLocation)(struct _ocrComponent_t *self, ocrLocation_t loc);
} ocrComponentFcts_t;

/*! \brief Abstract class to represent OCR component data structures.
 *
 */
typedef struct _ocrComponent_t {
    ocrGuid_t guid;         /**< The guid of this component */
    ocrComponentFcts_t fcts;
    ocrLocation_t mapping;  /**< Location mapping of this component */
    u64 count;
} ocrComponent_t;


/****************************************************/
/* OCR COMPONENT FACTORY                            */
/****************************************************/

/*! \brief Abstract factory class to create OCR components.
 *
 *  This class provides an interface to create Component instances with a non-static create function
 *  to allow runtime implementers to choose to have state in their derived ComponentFactory classes.
 */
typedef struct _ocrComponentFactory_t {
    /*! \brief Instantiates a Component and returns its corresponding GUID
     *
     */
    ocrComponent_t* (*instantiate)(struct _ocrComponentFactory_t * factory, ocrFatGuid_t hints, u32 properties);

    /*! \brief Virtual destructor for the ComponentFactory interface
     */
    void (*destruct)(struct _ocrComponentFactory_t * factory);

    ocrComponentFcts_t fcts;
} ocrComponentFactory_t;

#endif /* __OCR_COMPONENT_H_ */
