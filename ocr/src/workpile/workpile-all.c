/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "workpile/workpile-all.h"
#include "debug.h"

const char * workpile_types[] = {
#ifdef ENABLE_WORKPILE_HC
    "HC",
#endif
#ifdef ENABLE_WORKPILE_CE
    "CE",
#endif
#ifdef ENABLE_WORKPILE_XE
    "XE",
#endif
    NULL
};

ocrWorkpileFactory_t * newWorkpileFactory(workpileType_t type, ocrParamList_t * perType) {
    switch(type) {
#ifdef ENABLE_WORKPILE_HC
    case workpileHc_id:
        return newOcrWorkpileFactoryHc(perType);
#endif
#ifdef ENABLE_WORKPILE_CE
    case workpileCe_id:
        return newOcrWorkpileFactoryCe(perType);
#endif
#ifdef ENABLE_WORKPILE_XE
    case workpileXe_id:
        return newOcrWorkpileFactoryXe(perType);
#endif
    case workpileMax_id:
    default:
        ASSERT(0);
    }
    return NULL;
}

void initializeWorkpileOcr(ocrWorkpileFactory_t * factory, ocrWorkpile_t * self, ocrParamList_t *perInstance) {
    self->fguid.guid = UNINITIALIZED_GUID;
    self->fguid.metaDataPtr = self;
    self->pd = NULL;
    self->fcts = factory->workpileFcts;
}
