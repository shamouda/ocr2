/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef BIN_HEAP_H_
#define BIN_HEAP_H_

#include "ocr-config.h"
#include "ocr-types.h"
#include "ocr-policy-domain.h"

#ifndef INIT_BIN_HEAP_CAPACITY
// Set by configure
#   ifdef INIT_DEQUE_CAPACITY
#   define INIT_BIN_HEAP_CAPACITY (INIT_DEQUE_CAPACITY*16)
#   else
#   define INIT_BIN_HEAP_CAPACITY 32768
#   endif
#endif

/****************************************************/
/* BIN HEAP TYPES                                      */
/****************************************************/

/**
 * @brief Type of binHeaps
 */
typedef enum {
    // Virtual implementations
    NO_LOCK_BASE_BIN_HEAP       = 0x1,
    LOCK_BASE_BIN_HEAP          = 0x2,
    // Concrete implementations
    NON_CONCURRENT_BIN_HEAP     = 0x3,
    LOCKED_BIN_HEAP             = 0x4,
    MAX_BIN_HEAP_TYPE           = 0x5
} ocrBinHeapType_t;

/****************************************************/
/* BASE BIN HEAP                                       */
/****************************************************/

typedef struct {
    s64 priority;
    void *entry;
} ocrBinHeapEntry_t;

/**
 * @brief binary heap
 */
typedef struct _ocrBinHeap_t {
    ocrBinHeapType_t type;
    /* The fields don't need to be volatile because we only
       have non-concurrent and locking implementations. */
    u32 count;
    ocrBinHeapEntry_t *data;

    /** @brief Destruct binHeap
     */
    void (*destruct)(ocrPolicyDomain_t *pd, struct _ocrBinHeap_t *self);

    /** @brief Push element
     */
    void (*push)(struct _ocrBinHeap_t *self, void *entry, s64 priority, u8 doTry);

    /** @brief Pop element
     */
    void *(*pop)(struct _ocrBinHeap_t *self, u8 doTry);

} binHeap_t;

/****************************************************/
/* SINGLE LOCKED BIN_HEAP                              */
/****************************************************/

// binHeap with lock
typedef struct _ocrBinHeapLocked_t {
    binHeap_t base;
    volatile u32 lock;
} binHeapLocked_t;

/****************************************************/
/* BIN HEAP API                                        */
/****************************************************/

binHeap_t *newBinHeap(ocrPolicyDomain_t *pd, ocrBinHeapType_t type);

#endif /* BIN_HEAP_H_ */

