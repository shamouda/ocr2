/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#ifdef ENABLE_EXTENSION_AFFINITY

#include "debug.h"

#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "experimental/ocr-platform-model.h"
#include "experimental/ocr-placer.h"
#include "extensions/ocr-affinity.h"

//
// Internal Placement API
//

//Part of policy-domain debug messages
#define DEBUG_TYPE POLICY

// Assumptions:
// - neighbors are locations described as ranks [0:N[
// - neighbors contains all ranks but self
// - placer's affinities array represents all the PD locations and is sorted by rank id

//
// Begin platform model based on affinities
//

//Fetch remote affinity guid information
//BUG #162 metadata cloning: copy-paste from hc-dist-policy.c until we have MD cloning
static u8 resolveRemoteMetaData(ocrPolicyDomain_t * self, ocrFatGuid_t * fGuid, u64 metaDataSize) {
    ocrGuid_t remoteGuid = fGuid->guid;
    u64 val;
    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], remoteGuid, &val, NULL);
    if (val == 0) {
        DPRINTF(DEBUG_LVL_VVERB,"resolveRemoteMetaData: Query remote GUID metadata\n");
        // GUID is unknown, request a copy of the metadata
        PD_MSG_STACK(msgClone);
        getCurrentEnv(NULL, NULL, NULL, &msgClone);
#define PD_MSG (&msgClone)
#define PD_TYPE PD_MSG_GUID_METADATA_CLONE
        msgClone.type = PD_MSG_GUID_METADATA_CLONE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD_IO(guid.guid) = remoteGuid;
        PD_MSG_FIELD_IO(guid.metaDataPtr) = NULL;
        RESULT_ASSERT(self->fcts.processMessage(self, &msgClone, true), ==, 0)
        // On return, Need some more post-processing to make a copy of the metadata
        // and set the fatGuid's metadata ptr to point to the copy
        void * metaDataPtr = self->fcts.pdMalloc(self, metaDataSize);
        ASSERT(PD_MSG_FIELD_IO(guid.metaDataPtr) != NULL);
        ASSERT(ocrGuidIsEq(PD_MSG_FIELD_IO(guid.guid), remoteGuid));
        ASSERT(PD_MSG_FIELD_O(size) == metaDataSize);
        hal_memCopy(metaDataPtr, PD_MSG_FIELD_IO(guid.metaDataPtr), metaDataSize, false);
        //BUG #162 metadata cloning: Potentially multiple concurrent registerGuid on the same template
        self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], remoteGuid, (u64) metaDataPtr);
        val = (u64) metaDataPtr;
        DPRINTF(DEBUG_LVL_VVERB,"Data @ %p registered for GUID "GUIDF" for %"PRIu32"\n",
                metaDataPtr, GUIDA(remoteGuid), (u32)self->myLocation);
#undef PD_MSG
#undef PD_TYPE
        DPRINTF(DEBUG_LVL_VVERB,"resolveRemoteMetaData: Retrieved remote EDT template\n");
    }
    fGuid->metaDataPtr = (void *) val;
    return 0;
}

ocrLocation_t affinityToLocation(ocrGuid_t affinityGuid) {
    ocrFatGuid_t fguid;
    fguid.guid = affinityGuid;
    ocrPolicyDomain_t * pd;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    resolveRemoteMetaData(pd, &fguid, sizeof(ocrAffinity_t));
    ASSERT((fguid.metaDataPtr != NULL) && "ERROR: cannot deguidify affinity GUID");
    return ((ocrAffinity_t *) fguid.metaDataPtr)->place;
}

ocrPlatformModel_t * createPlatformModelAffinity(ocrPolicyDomain_t *pd) {
    ocrPlatformModelAffinity_t * model = pd->fcts.pdMalloc(pd, sizeof(ocrPlatformModelAffinity_t));
    u64 countAff = pd->neighborCount + 1;
    model->pdLocAffinities = NULL;
    model->pdLocAffinitiesSize = countAff;
    model->pdLocAffinities = pd->fcts.pdMalloc(pd, sizeof(ocrGuid_t)*countAff);
    // Returns an array of affinity where each affinity maps to a PD.
    // The array is ordered by PD's location (rank in mpi/gasnet)
    u64 i=0;
    for(i=0; i < pd->neighborCount; i++) {
        ASSERT(pd->neighbors[i] < countAff);
        ocrFatGuid_t fguid;
        pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY, GUID_PROP_NONE);
        ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->neighbors[i];
        model->pdLocAffinities[pd->neighbors[i]] = fguid.guid;
    }
    // Do current PD
    model->current = (u32)pd->myLocation;
    ocrFatGuid_t fguid;
    pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY, GUID_PROP_NONE);
    ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->myLocation;
    model->pdLocAffinities[model->current] = fguid.guid;

    for(i=0; i < countAff; i++) {
        DPRINTF(DEBUG_LVL_VVERB,"affinityGuid[%"PRId32"]="GUIDF"\n",
                (u32)i, GUIDA(model->pdLocAffinities[i]));
    }

    return (ocrPlatformModel_t *) model;
}


void destroyPlatformModelAffinity(ocrPolicyDomain_t *pd) {
    ocrPlatformModelAffinity_t * model = (ocrPlatformModelAffinity_t *) (pd->platformModel);
    u64 i=0;
    PD_MSG_STACK(msg);
    getCurrentEnv(NULL, NULL, NULL, &msg);
    for(i=0; i < pd->neighborCount; i++) {
#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
      msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
      PD_MSG_FIELD_I(guid.guid) = model->pdLocAffinities[pd->neighbors[i]];
      PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
      PD_MSG_FIELD_I(properties) = 1; // Free metadata
      pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE
    }

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_DESTROY
      msg.type = PD_MSG_GUID_DESTROY | PD_MSG_REQUEST;
      PD_MSG_FIELD_I(guid.guid) = model->pdLocAffinities[model->current];
      PD_MSG_FIELD_I(guid.metaDataPtr) = NULL;
      PD_MSG_FIELD_I(properties) = 1; // Free metadata
      pd->fcts.processMessage(pd, &msg, false);
#undef PD_MSG
#undef PD_TYPE

    pd->fcts.pdFree(pd, model->pdLocAffinities);
    pd->fcts.pdFree(pd, model);
    pd->platformModel = NULL;
}

//
// End platform model based on affinities
//

#endif /* ENABLE_EXTENSION_AFFINITY */
