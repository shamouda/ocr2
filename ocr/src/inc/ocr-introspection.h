// OCR Introspection defines

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_INTROSPECTION_H__
#define __OCR_INTROSPECTION_H__

typedef enum {
    /* Allocator queries */
    QUERY_ALLOC_MAXSIZE,        /**< Maximum size request that will succeed */
    QUERY_ALLOC_FREEBYTES,      /**< Number of free bytes in the memory */
    /* Computing platform/target queries */
    QUERY_COMP_CUR_TEMP,        /**< Current temperature of the computing resource */
    QUERY_COMP_CUR_FREQ,        /**< Current frequency of the computing resource */
    /* Marker */
    QUERY_MAX
} queryType_t;

#define QUERY_PROP_NULL              0x0  // Null property, default
#define QUERY_PROP_APPROX           0x10  // Query result can be approximate
#define QUERY_PROP_ALL             0x100  // All sibling modules to the one queried
                                          // should also be queried and the collective result
                                          // returned
#define QUERY_PROP_ANY             0x200  // Any sibling modules to the one queried
                                          // can return the result
#define QUERY_PROP_MAX             0x300  // All sibling modules to the one queried
                                          // are also queried and the maximum result returned
#define QUERY_PROP_MIN             0x400  // All sibling modules to the one queried
                                          // are also queried and the minimum result returned
#define QUERY_PROP_MEDIAN          0x500  // All sibling modules to the one queried
                                          // are also queried and the median result returned
#endif /* __OCR_INTROSPECTION_H__ */
