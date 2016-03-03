/**
 * @brief OCR internal MACROS for GUID management
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_GUID_MACROS_H__
#define __OCR_GUID_MACROS_H__

//#include "ocr-policy-domain.h"
#include "ocr-types.h"

/**
 * @brief wapper to fetch the kind of the object associated with a guid
 *
 * NOTE: This wrapper was introduced to support the GUID_KIND macro
 *       which require a call into a policy domain's guid provider.
 *       This may not be needed if discussion results in consensus
 *       that user need not ever be concerned with a guid's kind.
 *
 * @param[in] guid     The GUID of the object who's kind we want to fetch
 *
 * @return a code indicating kind associated with guid.
 **/

/* Macros for 64-bit GUID values */
#ifdef GUID_64

#define IS_GUID_NULL(g)                      \
    ((g).guid == 0x0)

#define IS_GUID_UNINITIALIZED(g)             \
    ((g).guid == -2)

#define IS_GUID_ERROR(g)                     \
    ((g).guid == -1)

#define IS_GUID_EQUAL(g1, g2)                   \
    ((g1).guid == (g2).guid)

#define IS_GUID_LESS_THAN(g1, g2)               \
    ((g1).guid < (g2).guid)

#define GUID_ASSIGN_VALUE(g1, g2)               \
    do {                                        \
        (g1).guid = (g2).guid;                  \
    } while(0)

#define GUIDFS(g) (g).guid

#define GUIDFL(g) (g).guid

#define GUIDSx "0x%lx"

#define GUIDLx "0x%lx"


/* Macros for 128-bit GUID values */
#elif defined(GUID_128)

#define IS_GUID_NULL(guid)                                              \
    ( ((guid).upper == 0x0) && ((guid).lower == 0x0) )

#define IS_GUID_UNINITIALIZED(guid)                                     \
    ( ((guid).upper == -2) && ((guid).lower == -2) )

#define IS_GUID_ERROR(guid)                                             \
    ( ((guid).upper == -1) && ((guid).lower == -1) )

#define IS_GUID_EQUAL(g1, g2)                                           \
    ( ((g1).lower == (g2).lower) && ( (g1).upper == (g2).upper) )

#define IS_GUID_LESS_THAN(g1, g2)                                       \
    ( ((g1).lower < (g2).lower) )                                       \

#define GUID_ASSIGN_VALUE(g1, g2)                                       \
    do {                                                                \
        (g1).lower = (g2).lower;                                        \
        (g1).upper = (g2).upper;                                        \
    } while(0)

#define GUIDFS(guid)                                                    \
    (guid).lower

#define GUIDFL(guid)                                                    \
    (guid).upper,(guid).lower

#define GUIDSx "0x%lx"

#define GUIDLx "0x%lx%lx"

#else
//Will never happen unless the preprocessor goes haywire
#error No GUID size specified

#endif

/* Common GUID macros */
#define GUID_SIZE(guid)             \
        (sizeof(guid)*8)            \

#endif /* __OCR_GUID_MACROS_H__ */

