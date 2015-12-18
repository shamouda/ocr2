/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */


#ifndef __OCR_RESILIENCY_H__
#define __OCR_RESILIENCY_H__

#include "ocr-runtime-types.h"
#include "ocr-types.h"

#ifdef OCR_ENABLE_STATISTICS
#include "ocr-statistics.h"
#endif

struct _ocrPolicyDomain_t;
struct _ocrPolicyMsg_t;
struct _ocrMsgHandle_t;

/****************************************************/
/* PARAMETER LISTS                                  */
/****************************************************/

typedef struct _paramListResiliencyFact_t {
    ocrParamList_t base;
} paramListResiliencyFact_t;

typedef struct _paramListResiliencyInst_t {
    ocrParamList_t base;
} paramListResiliencyInst_t;

/******************************************************/
/* OCR RESILIENCY                                     */
/******************************************************/

struct _ocrResiliency_t;
struct _ocrTask_t;

typedef struct _ocrResiliencyFcts_t {
    void (*destruct)(struct _ocrResiliency_t *self);

    /**
     * @brief Switch runlevel
     *
     * @param[in] self         Pointer to this object
     * @param[in] PD           Policy domain this object belongs to
     * @param[in] runlevel     Runlevel to switch to
     * @param[in] phase        Phase for this runlevel
     * @param[in] properties   Properties (see ocr-runtime-types.h)
     * @param[in] callback     Callback to call when the runlevel switch
     *                         is complete. NULL if no callback is required
     * @param[in] val          Value to pass to the callback
     *
     * @return 0 if the switch command was successful and a non-zero error
     * code otherwise. Note that the return value does not indicate that the
     * runlevel switch occured (the callback will be called when it does) but only
     * that the call to switch runlevel was well formed and will be processed
     * at some point
     */
    u8 (*switchRunlevel)(struct _ocrResiliency_t* self, struct _ocrPolicyDomain_t *PD, ocrRunlevel_t runlevel,
                         phase_t phase, u32 properties, void (*callback)(struct _ocrPolicyDomain_t*,u64), u64 val);

    /** @brief Invoke Resiliency Manager
     */
    u8 (*invoke)(struct _ocrResiliency_t *self, struct _ocrPolicyMsg_t *msg, u32 properties);

    /** @brief Notify Resiliency Manager of runtime op
     */
    u8 (*notify)(struct _ocrResiliency_t *self, struct _ocrPolicyMsg_t *msg, u32 properties);

    /** @brief Recover from fault
     */
    u8 (*recover)(struct _ocrResiliency_t *self, ocrFaultCode_t faultCode, ocrLocation_t loc, u8 *buffer);

} ocrResiliencyFcts_t;

typedef struct _ocrResiliency_t {
    ocrFatGuid_t fguid;
    ocrResiliencyFcts_t fcts;
} ocrResiliency_t;


/****************************************************/
/* OCR RESILIENCY FACTORY                           */
/****************************************************/

typedef struct _ocrResiliencyFactory_t {
    ocrResiliency_t* (*instantiate) (struct _ocrResiliencyFactory_t * factory, ocrParamList_t *perInstance);
    void (*destruct)(struct _ocrResiliencyFactory_t * factory);
    ocrResiliencyFcts_t fcts;
} ocrResiliencyFactory_t;

void initializeResiliencyOcr(ocrResiliencyFactory_t * factory, ocrResiliency_t * self, ocrParamList_t *perInstance);

#endif /* __OCR_RESILIENCY_H__ */
