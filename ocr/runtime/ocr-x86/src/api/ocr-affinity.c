/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-affinity.h"
#include "ocr-policy-domain.h"
#include "ocr-placer.h"

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
    } else if(kind == AFFINITY_PD_MASTER) {
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

    if (kind == AFFINITY_PD) {
        ASSERT(*count <= (pd->neighborCount + 1));
        //TODO don't want to invest in an affinity data-structure
        //until we clearly know what we want to do with tuning
        u64 i = 0;
        while(i < *count) {
            //TODO The affinity info is generated here, where should we read that from ?
            affinities[i] = (int) i;
            i++;
        }
    } else if (kind == AFFINITY_PD_MASTER) {
        //TODO This should likely come from the INI file
        //This is working on shmem because NULL_GUID is 0
        affinities[0] = (int) 0;
    } else {
        ASSERT(false && "Unknown affinity kind");
    }
    return 0;
}

//
// Internal Placement API
//

ocrLocation_t affinityToLocation(ocrGuid_t affinityGuid) {
    //TODO don't want to invest in an affinity data-structure
    //until we clearly know what we want to do with tuning
    return (ocrLocation_t) affinityGuid;
}

ocrPlacer_t * createLocationPlacer(ocrPolicyDomain_t *pd) {
    u64 countAff;
    ocrAffinityCount(AFFINITY_PD, &countAff);
    ocrLocationPlacer_t * placer = pd->fcts.pdMalloc(pd, sizeof(ocrLocationPlacer_t));
    placer->lock = 0;
    placer->edtLastPlacementIndex = 0;
    placer->pdLocAffinities = NULL;
    placer->pdLocAffinitiesSize = countAff;
    if (countAff > 0) {
        placer->pdLocAffinities = pd->fcts.pdMalloc(pd, sizeof(ocrGuid_t)*countAff);
        ocrAffinityGet(AFFINITY_PD, &countAff, placer->pdLocAffinities);
    }
    return (ocrPlacer_t *) placer;
}

u8 suggestLocationPlacement(ocrPolicyDomain_t *pd, ocrLocation_t curLoc, ocrLocationPlacer_t * placer, ocrPolicyMsg_t * msg) {
    u64 msgType = (msg->type & PD_MSG_TYPE_ONLY);
    //NOTE: the mainEDT
    // Incoming messages have been directed to the current location. Don't try to place them again.
    if ((msg->srcLocation == curLoc) && (placer != NULL) && (placer->pdLocAffinities != NULL)) {
        // Check if we need to place the DB/EDTs
        bool doPlace = false;
        switch(msgType) {
            case PD_MSG_WORK_CREATE:
            {
                #define PD_MSG msg
                #define PD_TYPE PD_MSG_WORK_CREATE
                    doPlace = ((PD_MSG_FIELD(workType) == EDT_USER_WORKTYPE) &&
                                (PD_MSG_FIELD(affinity.guid) == NULL_GUID));
                #undef PD_MSG
                #undef PD_TYPE
            break;
            }
            case PD_MSG_DB_CREATE:
            {
                #define PD_MSG msg
                #define PD_TYPE PD_MSG_DB_CREATE
                    doPlace = false;
                    // For now a DB is always created where the current EDT executes
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

        if (doPlace) {
            hal_lock32(&(placer->lock));
            int placementIndex = placer->edtLastPlacementIndex;
            ocrGuid_t pdLocAffinity = placer->pdLocAffinities[placementIndex];
            placer->edtLastPlacementIndex++;
            if (placer->edtLastPlacementIndex == placer->pdLocAffinitiesSize) {
                placer->edtLastPlacementIndex = 0;
            }
            hal_unlock32(&(placer->lock));
            switch(msgType) {
                case PD_MSG_WORK_CREATE:
                {
                    #define PD_MSG msg
                    #define PD_TYPE PD_MSG_WORK_CREATE
                        printf("[%d] EDT %ld Placement at %d \n", (int) pd->myLocation, PD_MSG_FIELD(guid.guid), placementIndex);
                        PD_MSG_FIELD(affinity.guid) = pdLocAffinity;
                    #undef PD_MSG
                    #undef PD_TYPE
                break;
                }
                case PD_MSG_DB_CREATE:
                {
                    #define PD_MSG msg
                    #define PD_TYPE PD_MSG_DB_CREATE
                        printf("[%d] DB %ld placement at %d \n", (int) pd->myLocation, PD_MSG_FIELD(guid.guid), placementIndex);
                        PD_MSG_FIELD(affinity.guid) = pdLocAffinity;
                    #undef PD_MSG
                    #undef PD_TYPE
                break;
                }
                default:
                    ASSERT(false);
            }
        }
    } // else do nothing: i.e. place locally

    return 0;
}
