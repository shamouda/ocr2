// OCR Runtime Hints

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef OCR_RUNTIME_HINT_H_
#define OCR_RUNTIME_HINT_H_

#include "ocr-types.h"
#include "ocr-hint.h"
#include "ocr-runtime-types.h"
#include "utils/ocr-utils.h"

//NOTE: Mainly placeholders

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/
typedef struct _paramListHintFact_t {
    ocrParamList_t base;
} paramListHintFact_t;

/****************************************************/
/* OCR USER HINTS UTILS                             */
/****************************************************/

extern u64 ocrHintPropIndexStart[];
extern u64 ocrHintPropIndexEnd[];

#define OCR_HINT_INDX(property, type)       (property - ocrHintPropIndexStart[type] - 1)
#define OCR_HINT_BIT_MASK(hint, property)   (0x1 << OCR_HINT_INDX(property, hint->type))
#define OCR_HINT_FIELD(hint, property)      ((u64*)(&(hint->args)))[OCR_HINT_INDX(property, hint->type)]

/****************************************************/
/* OCR RUNTIME HINTS                                */
/****************************************************/

#define RUNTIME_HINT_PROP_NULL  0 /* Initialization value */

typedef struct _ocrRuntimeHint_t {
    u32 properties;
} ocrRuntimeHint_t;

/****************************************************/
/* OCR RUNTIME HINT FACTORY                         */
/****************************************************/

typedef struct _ocrHintFcts_t {
    /**
     * @brief Size of a hint object
     *        This returns the sizeof the hint implementation
     *        for a specific hint module. Since OCR object hints
     *        do not have guids, they are allocated as part of the
     *        OCR object it is associated to. This allows the
     *        object allocation to know the size required in the
     *        metadata to hold the hint.
     *
     * @param[in] hintType      Type of hint
     */
    //u64 (*sizeofHint)(ocrHintTypes_t hintType);

    /**
     * @brief Process incoming user hints for runtime
     *        This calls into the runtime hint module
     *        which may process or discard the hint.
     *
     * @param[in] uHint          Pointer to user hint
     * @param[in] rHint          Pointer to runtime hint
     */
    //u8 (*processUserHint)(ocrHint_t* uHint, ocrRuntimeHint_t* rHint);

    /**
     * @brief Process incoming system hints for runtime
     *        System hints are triggered by the
     *        introspection module.
     *        This calls into the runtime hint module
     *        which may process or discard the hint.
     *
     * @param[in] sHint          Pointer to system hint
     * @param[in] rHint          Pointer to runtime hint
     */
    //u8 (*processSystemHint)(ocrSysHint_t* sHint, ocrRuntimeHint_t* rHint);

} ocrHintFcts_t;

typedef struct _ocrHintFactory_t {
    /*! \brief Virtual destructor for the HintFactory
     */
    void (*destruct)(struct _ocrHintFactory_t * factory);

    ocrHintFcts_t fcts; /**< Functions for all hints */

    u32 factoryId;
} ocrHintFactory_t;

#endif /* OCR_RUNTIME_HINT_H_ */
