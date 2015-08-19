/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __POLICY_DOMAIN_ALL_H_
#define __POLICY_DOMAIN_ALL_H_

#include "debug.h"
#include "ocr-config.h"
#include "ocr-policy-domain.h"
#include "utils/ocr-utils.h"

typedef enum _policyDomainType_t {
#ifdef ENABLE_POLICY_DOMAIN_HC
    policyDomainHc_id,
#endif
#ifdef ENABLE_POLICY_DOMAIN_HC_DIST
    policyDomainHcDist_id,
#endif
#ifdef ENABLE_POLICY_DOMAIN_XE
    policyDomainXe_id,
#endif
#ifdef ENABLE_POLICY_DOMAIN_CE
    policyDomainCe_id,
#endif
    policyDomainMax_id
} policyDomainType_t;

extern const char * policyDomain_types [];

#ifdef ENABLE_POLICY_DOMAIN_HC
#include "policy-domain/hc/hc-policy.h"
#endif
#ifdef ENABLE_POLICY_DOMAIN_HC_DIST
#include "policy-domain/hc-dist/hc-dist-policy.h"
#endif
#ifdef ENABLE_POLICY_DOMAIN_XE
#include "policy-domain/xe/xe-policy.h"
#endif
#ifdef ENABLE_POLICY_DOMAIN_CE
#include "policy-domain/ce/ce-policy.h"
#endif

ocrPolicyDomainFactory_t * newPolicyDomainFactory(policyDomainType_t type, ocrParamList_t *perType);

#endif /* __POLICY_DOMAIN_ALL_H_ */
