/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#include "extensions/ocr-resiliency-sim.h"
#include "resiliency/null/null-resiliency.h"


#include "ocr-resiliency.h"

#include "ocr-types.h"
#include "ocr-runtime-types.h"
#include "policy-domain/hc/hc-policy.h"

#include "ocr-errors.h"

u8 ocrInjectFault(ocrFaultSimCode_t code, void *faultData) {
    ocrPolicyDomain_t *pd;
    ocrWorker_t *worker;
    getCurrentEnv(&pd, &worker, NULL, NULL);
    ocrLocation_t loc = (ocrLocation_t) worker->id;

    ocrFaultCode_t faultCode = OCR_FAULT_MAX;
    switch(code) {
    case OCR_FAULT_SIM_FREQUENCY:
        faultCode = OCR_FAULT_FREQUENCY;
        break;
    default:
        return OCR_ENOTSUP;
    }

    ocrResiliency_t *rm = pd->resiliency[0];
    u32 returnCode = rm->fcts.recover(rm, faultCode, loc, (u8*)faultData);
    ASSERT(returnCode == 0);
    return 0;
}

