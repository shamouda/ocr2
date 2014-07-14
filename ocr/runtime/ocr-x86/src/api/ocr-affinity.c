/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-affinity.h"
#include "ocr-policy-domain.h"
#include "ocr-placer.h"

//
// Internal Placement API
//

//Part of policy-domain debug messages
#define DEBUG_TYPE POLICY

// Location Placer Implementation
// NOTE: This implementation is not the fruit of any thinking
//
// Assumptions:
// - neighbors are locations described as ranks [0:N[
// - neighbors contains all ranks but self
// - placer's affinities array represents all the PD locations and is sorted by rank id

ocrGuid_t locationToAffinity(ocrLocationPlacer_t * placer, ocrLocation_t location) {
    //Warning: assumes locations are rank which maps to an index in the location placer.
    u32 idx = (u32)location;
    ASSERT(idx < placer->pdLocAffinitiesSize);
    return placer->pdLocAffinities[idx];
}

//TODO nasty copy from hc-dist-policy.c until we don't have proper remote guid metdata resolve impl in PD
static u8 resolveRemoteMetaData(ocrPolicyDomain_t * self, ocrFatGuid_t * fGuid, u64 metaDataSize) {
    ocrGuid_t remoteGuid = fGuid->guid;
    u64 val;
    self->guidProviders[0]->fcts.getVal(self->guidProviders[0], remoteGuid, &val, NULL);
    if (val == 0) {
        DPRINTF(DEBUG_LVL_VVERB,"[%d] resolveRemoteMetaData: Query remote GUID metadata\n", (u32)self->myLocation);
        // GUID is unknown, request a copy of the metadata
        ocrPolicyMsg_t msgClone;
        getCurrentEnv(NULL, NULL, NULL, &msgClone);
        #define PD_MSG (&msgClone)
        #define PD_TYPE PD_MSG_GUID_METADATA_CLONE
            msgClone.type = PD_MSG_GUID_METADATA_CLONE | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
            PD_MSG_FIELD(guid.guid) = remoteGuid;
            PD_MSG_FIELD(guid.metaDataPtr) = NULL;
            u8 returnCode = self->fcts.processMessage(self, &msgClone, true);
            ASSERT(returnCode == 0);
            // On return, Need some more post-processing to make a copy of the metadata
            // and set the fatGuid's metadata ptr to point to the copy
            void * metaDataPtr = self->fcts.pdMalloc(self, metaDataSize);
            hal_memCopy(metaDataPtr, &PD_MSG_FIELD(metaData), metaDataSize, false);
            //DIST-TODO Potentially multiple concurrent registerGuid on the same template
            self->guidProviders[0]->fcts.registerGuid(self->guidProviders[0], remoteGuid, (u64) metaDataPtr);
            val = (u64) metaDataPtr;
            DPRINTF(DEBUG_LVL_VVERB,"[%d] GUID registered for %ld\n", (u32)self->myLocation, remoteGuid);
        #undef PD_MSG
        #undef PD_TYPE
        DPRINTF(DEBUG_LVL_VVERB,"[%d] resolveRemoteMetaData: Retrieved remote EDT template\n", (u32)self->myLocation);
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

ocrPlacer_t * createLocationPlacer(ocrPolicyDomain_t *pd) {
    u64 countAff = pd->neighborCount + 1;
    ocrLocationPlacer_t * placer = pd->fcts.pdMalloc(pd, sizeof(ocrLocationPlacer_t));
    placer->lock = 0;
    placer->edtLastPlacementIndex = 0;
    placer->pdLocAffinities = NULL;
    placer->pdLocAffinitiesSize = countAff;
    placer->pdLocAffinities = pd->fcts.pdMalloc(pd, sizeof(ocrGuid_t)*countAff);
    // Returns an array of affinity where each affinity maps to a PD.
    // The array is ordered by PD's location (rank in mpi/gasnet)
    u64 i=0;
    for(i=0; i < pd->neighborCount; i++) {
        ASSERT(pd->neighbors[i] < countAff);
        ocrFatGuid_t fguid;
        pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY);
        ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->neighbors[i];
        placer->pdLocAffinities[pd->neighbors[i]] = fguid.guid;
    }
    // Do current PD
    placer->current = (u32)pd->myLocation;
    ocrFatGuid_t fguid;
    pd->guidProviders[0]->fcts.createGuid(pd->guidProviders[0], &fguid, sizeof(ocrAffinity_t), OCR_GUID_AFFINITY);
    ((ocrAffinity_t*)fguid.metaDataPtr)->place = pd->myLocation;
    placer->pdLocAffinities[placer->current] = fguid.guid;

    for(i=0; i < countAff; i++) {
        DPRINTF(DEBUG_LVL_VVERB,"[%d] affinityGuid[%d]=0x%lx\n", (u32)pd->myLocation, (u32)i, placer->pdLocAffinities[i]);
    }

    return (ocrPlacer_t *) placer;
}

u8 suggestLocationPlacement(ocrPolicyDomain_t *pd, ocrLocation_t curLoc, ocrLocationPlacer_t * placer, ocrPolicyMsg_t * msg) {
    u64 msgType = (msg->type & PD_MSG_TYPE_ONLY);
    // Incoming messages have been directed to the current location. Don't try to place them again.

    // Only try to place message the current policy domain is generating (i.e. from executing user code)
    if ((msg->srcLocation == curLoc) && (msg->destLocation == curLoc) && 
        (placer != NULL) && (placer->pdLocAffinities != NULL)) {
        // Check if we need to place the DB/EDTs
        bool doAutoPlace = false;
        switch(msgType) {
            case PD_MSG_WORK_CREATE:
            {
            #define PD_MSG msg
            #define PD_TYPE PD_MSG_WORK_CREATE
                doAutoPlace = (PD_MSG_FIELD(workType) == EDT_USER_WORKTYPE) && 
                          (PD_MSG_FIELD(affinity.guid) == NULL_GUID);
                if (PD_MSG_FIELD(affinity.guid) != NULL_GUID) {
                    msg->destLocation = affinityToLocation(PD_MSG_FIELD(affinity.guid));
                }
            #undef PD_MSG
            #undef PD_TYPE
            break;
            }
            case PD_MSG_DB_CREATE:
            {
            #define PD_MSG msg
            #define PD_TYPE PD_MSG_DB_CREATE
                doAutoPlace = false;
                // For now a DB is always created where the current EDT executes unless
                // it has an affinity specified (i.e. no auto-placement)
                if (PD_MSG_FIELD(affinity.guid) != NULL_GUID) {
                    msg->destLocation = affinityToLocation(PD_MSG_FIELD(affinity.guid));
                }
                //TODO when we do place DBs make sure we only place USER DBs
                // doPlace = ((PD_MSG_FIELD(dbType) == USER_DBTYPE) &&
                //             (PD_MSG_FIELD(affinity.guid) == NULL_GUID));
            #undef PD_MSG
            #undef PD_TYPE
            break;
            }
            default:
                // Fall-through
            break;
        }

        if (doAutoPlace) {
            hal_lock32(&(placer->lock));
            u32 placementIndex = placer->edtLastPlacementIndex;
            ocrGuid_t pdLocAffinity = placer->pdLocAffinities[placementIndex];
            placer->edtLastPlacementIndex++;
            if (placer->edtLastPlacementIndex == placer->pdLocAffinitiesSize) {
                placer->edtLastPlacementIndex = 0;
            }
            hal_unlock32(&(placer->lock));
            msg->destLocation = affinityToLocation(pdLocAffinity);
            DPRINTF(DEBUG_LVL_VVERB,"[%d] Auto-Placement for msg %p, type 0x%x, at location %d\n", 
                (u32)pd->myLocation, msg, msgType, (u32)placementIndex);
        }
    }

    return 0;
}


//
// Affinity group API
//

// These allow to handle the simple case where the caller request a number
// of affinity guids and does the mapping. i.e. assign some computation
// and data to each guid.

u8 ocrAffinityCount(ocrAffinityKind kind, u64 * count) {
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    if (kind == AFFINITY_PD) {
        //TODO this is assuming each PD knows about every other PDs
        //Need to revisit that when we have a better idea of what affinities are
        *count = (pd->neighborCount + 1);
    } else if ((kind == AFFINITY_PD_MASTER) || (kind == AFFINITY_CURRENT)) {
        // Change this implementation if 'AFFINITY_CURRENT' cardinality can be > 1
        *count = 1;
    } else {
        ASSERT(false && "Unknown affinity kind");
    }
    return 0;
}

//TODO This call returns affinities with identical mapping across PDs.
u8 ocrAffinityGet(ocrAffinityKind kind, u64 * count, ocrGuid_t * affinities) {
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    ocrLocationPlacer_t * placer = ((ocrLocationPlacer_t*)pd->placer);
    if (kind == AFFINITY_PD) {
        ASSERT(*count <= (pd->neighborCount + 1));
        u64 i = 0;
        while(i < *count) {
            affinities[i] = placer->pdLocAffinities[i];
            i++;
        }
    } else if (kind == AFFINITY_PD_MASTER) {
        //TODO This should likely come from the INI file
        affinities[0] = placer->pdLocAffinities[0];
    } else if (kind == AFFINITY_CURRENT) {
        affinities[0] = placer->pdLocAffinities[placer->current];
    } else {
        ASSERT(false && "Unknown affinity kind");
    }
    return 0;
}

u8 ocrAffinityGetCurrent(ocrGuid_t * affinity) {
    u64 count = 1;
    return ocrAffinityGet(AFFINITY_CURRENT, &count, affinity);
}
