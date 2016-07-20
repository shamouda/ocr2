/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_DIST_POLICY_H__
#define __HC_DIST_POLICY_H__

#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_HC_DIST

#include "ocr-policy-domain.h"
#include "utils/ocr-utils.h"
#include "utils/hashtable.h"
#include "policy-domain/hc/hc-policy.h"

#include "event/hc/hc-event.h"
/******************************************************/
/* OCR-HC DISTRIBUTED POLICY DOMAIN                   */
/******************************************************/

typedef struct {
    ocrPolicyDomainHc_t base;
    u8 (*baseProcessMessage)(struct _ocrPolicyDomain_t *self, struct _ocrPolicyMsg_t *msg,
                             u8 isBlocking);
    u8 (*baseSwitchRunlevel)(struct _ocrPolicyDomain_t *self, ocrRunlevel_t, u32);
    u64 shutdownAckCount;
    u32 lockDbLookup;  /**< Lock for querying proxies for remote DB */
    u32 lockTplLookup; /**< Lock for querying proxies for remote template */
    hashtable_t * proxyTplMap;

    //ULFM Resilience
    ResEventNode_t* proxyListHead;
    ResEventNode_t* proxyListTail;
    u32 lockResEvtList; // Lock for proxy events list 
    ocrLocation_t * deadLocations;
    u32 deadLocationsCount;
    
} ocrPolicyDomainHcDist_t;

typedef struct {
    ocrPolicyDomainFactory_t base;
    void (*baseInitialize) (struct _ocrPolicyDomainFactory_t *factory, struct _ocrPolicyDomain_t* self,
#ifdef OCR_ENABLE_STATISTICS
         ocrStats_t *statsObject,
#endif
         ocrParamList_t *perInstance);
    u8 (*baseProcessMessage)(struct _ocrPolicyDomain_t *self, struct _ocrPolicyMsg_t *msg,
                         u8 isBlocking);
    u8 (*baseSwitchRunlevel)(struct _ocrPolicyDomain_t *self, ocrRunlevel_t, u32);
} ocrPolicyDomainFactoryHcDist_t;

ocrPolicyDomainFactory_t *newPolicyDomainFactoryHcDist(ocrParamList_t *perType);

#endif /* ENABLE_POLICY_DOMAIN_HC_DIST */
#endif /* __HC_DIST_POLICY_H__ */
