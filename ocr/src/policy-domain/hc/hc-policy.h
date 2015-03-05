/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __HC_POLICY_H__
#define __HC_POLICY_H__

#include "ocr-config.h"
#ifdef ENABLE_POLICY_DOMAIN_HC

#include "ocr-policy-domain.h"
#include "utils/ocr-utils.h"

/******************************************************/
/* OCR-HC POLICY DOMAIN                               */
/******************************************************/

typedef struct {
    ocrPolicyDomainFactory_t base;
} ocrPolicyDomainFactoryHc_t;

typedef struct {
    ocrPolicyDomain_t base;
    u32 rank;           // For MPI use
    u32 rl; // current RL
    u32 * rlNbDownActions; // size is nb of runlevel
    u32 ** rlDownActions; // size is nb of runlevel, and entries size is rlNbDownActions
    u32 * volatile rl_completed; //TODO-RL makes sense ?
} ocrPolicyDomainHc_t;

typedef struct {
    paramListPolicyDomainInst_t base;
    u32 rank;
} paramListPolicyDomainHcInst_t;

ocrPolicyDomainFactory_t *newPolicyDomainFactoryHc(ocrParamList_t *perType);

ocrPolicyDomain_t * newPolicyDomainHc(ocrPolicyDomainFactory_t * policy,
#ifdef OCR_ENABLE_STATISTICS
                                      ocrStats_t *statsObject,
#endif
                                      ocrCost_t *costFunction, ocrParamList_t *perInstance);

#endif /* ENABLE_POLICY_DOMAIN_HC */
#endif /* __HC_POLICY_H__ */
