/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_EVENT_H__
#define __HC_EVENT_H__

#include "ocr-config.h"
#ifdef ENABLE_EVENT_HC

#include "hc/hc.h"
#include "ocr-event.h"
#include "ocr-types.h"
#include "utils/ocr-utils.h"

#ifdef ENABLE_HINTS
/**< The number of hint properties supported by this implementation */
#define OCR_HINT_COUNT_EVT_HC   0
#else
#define OCR_HINT_COUNT_EVT_HC   0
#endif

// Size for the static waiter array
#ifndef HCEVT_WAITER_STATIC_COUNT
#define HCEVT_WAITER_STATIC_COUNT 4
#endif

// Size for the dynamically allocated waiter array
#ifndef HCEVT_WAITER_DYNAMIC_COUNT
#define HCEVT_WAITER_DYNAMIC_COUNT 4
#endif

typedef struct {
    ocrEventFactory_t base;
} ocrEventFactoryHc_t;

typedef struct ocrEventHc_t {
    ocrEvent_t base;
    regNode_t waiters[HCEVT_WAITER_STATIC_COUNT]; /**< hold waiters. If overflows a dynamically
                                              allocated waiter list is stored in waitersDb */
    ocrFatGuid_t waitersDb; /**< DB containing an array of regNode_t listing the
                             * events/EDTs depending on this event */
    u32 waitersCount; /**< Number of waiters in waitersDb */
    u32 waitersMax; /**< Maximum number of waiters in waitersDb */
    volatile u32 waitersLock;
    ocrRuntimeHint_t hint;
} ocrEventHc_t;

typedef struct _ocrEventHcPersist_t {
    ocrEventHc_t base;
    ocrGuid_t data;
} ocrEventHcPersist_t;

typedef struct ocrEventHcLatch_t {
    ocrEventHc_t base;
    volatile s32 counter;
} ocrEventHcLatch_t;

ocrEventFactory_t* newEventFactoryHc(ocrParamList_t *perType, u32 factoryId);

#endif /* ENABLE_EVENT_HC */
#endif /* __HC_EVENT_H__ */
