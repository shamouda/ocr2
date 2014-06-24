// OCR Hints

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_HINT_H_
#define OCR_HINT_H_

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "utils/ocr-utils.h"

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/
typedef struct _paramListHintFact_t {
    ocrParamList_t base;
} paramListHintFact_t;

/****************************************************/
/* OCR RUNTIME HINTS PARAMETER LIST                 */
/****************************************************/

typedef enum {
    HINT_PARAM_NONE = 0,
    HINT_PARAM_ALLOC_DB,
    HINT_PARAM_ALLOC_EDT,
    HINT_PARAM_SCHED_EDT,
    HINT_PARAM_SATISFY_EDT,
} ocrHintParam_t;

typedef struct _paramListHint_t {
    ocrHintParam_t type;
} ocrParamListHint_t;

typedef enum {
    HINT_INTROSPECTION_NONE = 0,
    HINT_INTROSPECTION_SATISFY_EDT,
} ocrHintIntrospection_t;

/****************************************************/
/* OCR USER HINT PROPERTIES                         */
/****************************************************/

#define HINT_PROP_OP(op) (op & (~(0xF)))

typedef enum {
    /* common hint props LS 4 bits */
    HINT_PROP_NONE              = 0x0,
    HINT_PROP_USER              = 0x1,
    HINT_PROP_RUNTIME           = 0x2,
    /* start all prop for hint ops */
    HINT_PROP_SET_PRIORITY      = 0x10,
    HINT_PROP_GET_PRIORITY      = 0x20,
    HINT_PROP_SET_AFFINITY      = 0x30,
    HINT_PROP_GET_AFFINITY      = 0x40,
    HINT_PROP_SET_DEP_WEIGHT    = 0x50,
    HINT_PROP_GET_DEP_WEIGHT    = 0x60,
} ocrHintProp;

/****************************************************/
/* OCR USER HINTS                                   */
/****************************************************/

typedef struct _ocrHint_t {
    ocrGuid_t guid;         /**< GUID for this hint */
    ocrHintTypes_t type;
} ocrHint_t;

/*! \brief EDT Template Hint implementation for OCR
 */
typedef struct _ocrHintEdtTemp_t {
    ocrHint_t base;
    u32 edtPriority;
    u32 dominantDbSlot;
    u32 dominantDbWeight;
} ocrHintEdtTemp_t;

/*! \brief EDT Hint implementation for OCR
 */
typedef struct _ocrHintEdt_t {
    ocrHint_t base;
    u32 priority;
    u32 dominantDbSlot;
    ocrLocation_t mapping;
    ocrGuid_t affinity;
} ocrHintEdt_t;

/****************************************************/
/* OCR USER HINT FACTORY                            */
/****************************************************/

typedef struct _ocrHintFcts_t {
    /**
     * @brief Virtual destructor for the Hint interface
     * @param[in] self          Pointer to this hint
     */
    u8 (*destruct)(struct _ocrHint_t* self);

    /**
     * @brief Initialize hint and (optionally) populate
     *        with data from another
     * @param[in] self          Pointer to this hint
     * @param[in] type          Type of hint
     * @param[in] properties    Hint specific properties
     * @param[in] other         Pointer to other hint
     */
    u8 (*initialize)(struct _ocrHint_t* self, ocrHintTypes_t type, u32 properties, struct _ocrHint_t* other);

    /**
     * @brief Populate hint with data from another
     * @param[in] self          Pointer to this hint
     * @param[in] other         Pointer to other hint
     */
    u8 (*setHint)(struct _ocrHint_t* self, struct _ocrHint_t* other);

    /**
     * @brief Get a new hint object populated with data from this hint
     * @param[in] self          Pointer to this hint
     */
    struct _ocrHint_t* (*getHint)(struct _ocrHint_t* self);

    /**
     * @brief Size of a hint object
     *        This is required when hints get allocated
     *        as part of another object, e.g. EDT hints
     *
     * @param[in] type          Type of hint
     * @param[in] properties    Hint specific properties
     */
    u32 (*sizeofHint)(ocrHintTypes_t type, u32 properties);

    /**
     * @brief Hint interface to set priority
     * @param[in] self          Pointer to this hint
     */
    u8 (*setPriority)(struct _ocrHint_t* self, u32 priority);

    /**
     * @brief Hint interface to get priority
     * @param[in] self          Pointer to this hint
     */
    u8 (*getPriority)(struct _ocrHint_t* self, u32* priority);

    /**
     * @brief Hint interface to set affinity
     * @param[in] self          Pointer to this hint
     * @param[in] affinity      Affinity guid of this hint
     */
    u8 (*setAffinity)(struct _ocrHint_t* self, ocrGuid_t affinity);

    /**
     * @brief Hint interface to get affinity
     * @param[in] self          Pointer to this hint
     * @param[out] affinity     Affinity guid of this hint
     */
    u8 (*getAffinity)(struct _ocrHint_t* self, ocrGuid_t* affinity);

    /**
     * @brief Hint interface to set dep weight
     * @param[in] self          Pointer to this hint
     * @param[in] slot          Slot number of dependence
     * @param[in] weight        Weight on that dependence (0-100)
     */
    u8 (*setDepWeight)(struct _ocrHint_t* self, u32 slot, u32 weight);

    /**
     * @brief Hint interface to set dep weight
     * @param[in] self          Pointer to this hint
     * @param[in/out] slot      if (slot == NULL) returns the slot with max weight
     *                          else if a value is passed, then we return the
     *                          weight for that slot
     * @param[out] weight
     */
    u8 (*getDepWeight)(struct _ocrHint_t* self, u32* slot, u32* weight);

} ocrHintFcts_t;

typedef struct _ocrHintFactory_t {
    /*! \brief Instantiates an Hint and returns its corresponding GUID
     */
    ocrHint_t* (*instantiate)(struct _ocrHintFactory_t * factory, ocrHintTypes_t hintType, u32 properties);

    /*! \brief Virtual destructor for the HintFactory
     */
    void (*destruct)(struct _ocrHintFactory_t * factory);

    ocrHintFcts_t fcts; /**< Functions for all hints */

    u32 factoryId;
} ocrHintFactory_t;

#endif /* OCR_HINT_H_ */
