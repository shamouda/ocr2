/**
 * @brief OCR internal functions for GUID management
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_GUID_FUNCTIONS_H__
#define __OCR_GUID_FUNCTIONS_H__

#include "ocr-types.h"

/**
 * @defgroup OCRGuid GUID management for OCR
 * @brief Describes the GUID API for OCR
 *
 * GUIDs are opaque data-types that uniquely identify an OCR object. Note that
 * GUIDs are not necessarily unique over the lifetime of the program but, at any
 * given point, a GUID uniquely identifies an OCR object.
 *
 * Since GUIDs are opaque, the functions provided below must be used to manipulate
 * them. GUIDs can, however, be assigned to one another (ie: the '=' operator
 * works as expected). #GUID_BIT_COUNT will return the number of bits a GUID
 * occupies but no other assumption on the GUID may be made.
 * @{
 **/

/* Macros for 64-bit GUID values */
#if GUID_BIT_COUNT == 64
/**
 * @brief Tests whether a GUID is #NULL_GUID
 *
  * @param[in] guid  GUID to test
 * @return true if the GUID is equal to #NULL_GUID or
 * false otherwise
 */
static inline bool ocrGuidIsNull(ocrGuid_t guid){
    return (guid.guid == 0x0);
}

/**
 * @brief Tests whether a GUID is #UNINITIALIZED_GUID
 *
 * @param[in] guid  GUID to test
 * @return true if the GUID is equal to #UNINITIALIZED_GUID or
 * false otherwise
 */
static inline bool ocrGuidIsUninitialized(ocrGuid_t guid){
    return (guid.guid == -2);
}

/**
 * @brief Tests whether a GUID is #ERROR_GUID
 *
 * @param[in] guid  GUID to test
 * @return true if the GUID is equal to #ERROR_GUID or
 * false otherwise
 */
static inline bool ocrGuidIsError(ocrGuid_t guid){
    return (guid.guid == -1);
}


/**
 * @brief Tests whether two GUIDs are equal
 *
 * Note that this is the only way to test equality
 * and converting a GUID to bytes and testing
 * for equality may not produce the correct result.
 *
 * @param[in] g1    GUID to test
 * @param[in] g2    GUID to test
 * @return true if g1 and g2 represent the same GUID
 * false otherwise
 */
static inline bool ocrGuidIsEq(ocrGuid_t g1, ocrGuid_t g2){
    return g1.guid == g2.guid;
}

/**
 * @brief Tests whether one GUID is less than another
 *
 * While the value of a GUID is usually not important, this
 * function can be used to sort GUIDs if the user wishes to
 *
 * @param[in] g1    GUID to test
 * @param[in] g2    GUID to test
 * @return true if g1 represents a GUID that is smaller than g2
 * false otherwise
 */
static inline bool ocrGuidIsLt(ocrGuid_t g1, ocrGuid_t g2){
    return g1.guid < g2.guid;
}

/* Macros for printing 64-bit GUIDS */
/**
 * @brief Argument specifier when printing a GUID
 *
 * Since GUIDs are opaque, printing them requires both
 * a format specifier (#GUIDF) and an argument specifier
 * (#GUIDA). As an example: PRINTF("This is a GUID"GUIDF"\n", GUIDA(guid))
 * will print the GUID guid.
 */
#define GUIDA(v) ((v).guid)

/**
 * @brief Format specifier when printing a GUID
 * @see #GUIDA
 */
#define GUIDF "0x%"PRIxPTR

/**
 * @}
 */

/* Macros for 128-bit GUID values */
#elif GUID_BIT_COUNT == 128
static inline bool ocrGuidIsNull(ocrGuid_t guid){
    return ((guid.upper == 0x0) && (guid.lower == 0x0));
}

static inline bool ocrGuidIsUninitialized(ocrGuid_t guid){
    return ((guid.upper == -2) && (guid.lower == -2));
}

static inline bool ocrGuidIsError(ocrGuid_t guid){
    return ((guid.upper == -1) && (guid.lower == -1));
}

static inline bool ocrGuidIsEq(ocrGuid_t g1, ocrGuid_t g2){
    return ((g1.lower == g2.lower) && (g1.upper == g2.upper));
}

static inline bool ocrGuidIsLt(ocrGuid_t g1, ocrGuid_t g2){
    return (g1.lower < g2.lower);
}

/*Macros for printing 128-bit GUIDS */
#define GUIDA(v) ((v).lower)
#define GUIDF "0x%"PRIxPTR

#else
//Will never happen unless the preprocessor goes haywire
#error Invalid GUID size specified

#endif

/* Legacy support */
/* WARNING: All these calls are depcrecated. Please use the ones above */
static inline bool IS_GUID_NULL(ocrGuid_t guid){ return ocrGuidIsNull(guid); }
static inline bool IS_GUID_UNINITIALIZED(ocrGuid_t guid){ return ocrGuidIsUninitialized(guid); }
static inline bool IS_GUID_ERROR(ocrGuid_t guid){ return ocrGuidIsError(guid); }
static inline bool IS_GUID_EQUAL(ocrGuid_t g1, ocrGuid_t g2){ return ocrGuidIsEq(g1, g2); }
static inline bool IS_GUID_LESS_THAN(ocrGuid_t g1, ocrGuid_t g2){ return ocrGuidIsLt(g1, g2); }

#define GUID_ASSIGN_VALUE(g1, g2) ((g1) = (g2))

#define GUIDSx GUIDF
#define GUIDLx GUIDF

#define GUIDFS(g) GUIDA(g)
#define GUIDFL(g) GUIDA(g)

#define GUID_SIZE(guid) (sizeof(guid)*8)

#endif /* __OCR_GUID_FUNCTIONS_H__ */

