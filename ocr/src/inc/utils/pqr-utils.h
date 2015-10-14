#include "ocr-config.h"
#ifdef ENABLE_EXTENSION_PAUSE

#include "ocr-types.h"
#include "policy-domain/hc/hc-policy.h"

ocrGuid_t hcQueryNextEdts(ocrPolicyDomainHc_t *self, void **result, u32 *size);

ocrGuid_t hcQueryAllEdts(ocrPolicyDomainHc_t *rself, void ** result, u32 *size);

ocrGuid_t hcDumpNextEdt(ocrWorker_t *worker, u32 *size);

ocrGuid_t hcQueryPreviousDatablock(ocrPolicyDomainHc_t *rself, void **result, u32 *size);

#endif
