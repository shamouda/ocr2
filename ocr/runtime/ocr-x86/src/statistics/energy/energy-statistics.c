/**
 * @brief Implementation of a default statistics collector
 **/

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifdef OCR_ENABLE_STATISTICS

#include "debug.h"
#include "ocr-macros.h"
#include "ocr-policy-domain-getter.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "ocr-utils.h"
#include "statistics/filters/filters-all.h"
#include "statistics/internal.h"
#include "statistics/messages/messages-all.h"

#include "energy-statistics.h"

#define DEBUG_TYPE STATS

/******************************************************/
/* OCR STATISTICS DEFAULT IMPLEMENTATION              */
/******************************************************/

static void energyDestruct(ocrStats_t *self) {
    ocrStatsEnergy_t *rself = (ocrStatsEnergy_t*)self;
    if(rself->aggregatingFilter)
        rself->aggregatingFilter->fcts.destruct(rself->aggregatingFilter);
    free(self);
}

static void energySetPD(ocrPolicyDomain_t *pd) {
    // Do nothing for now
}

static ocrStatsMessage_t* energyCreateMessage(ocrStats_t *self,
                                              ocrStatsEvt_t type,
                                              ocrGuid_t src, ocrGuid_t dest,
                                              ocrStatsParam_t *instanceArg) {

    // In the general case, you can switch on 'type' and pick an ocrStatsEvtInt_t
    // that is appropriate
    switch (type) {
        case STATS_EDT_START:
        case STATS_EDT_END:
        case STATS_DB_ACQ:
        case STATS_DB_REL:
            return newStatsMessage(STATS_MESSAGE_ENERGY, type, src, dest, instanceArg);
        default:
            return NULL;
    }
}

static void energyDestructMessage(ocrStats_t *self, ocrStatsMessage_t* message) {

    message->fcts.destruct(message);
}

static ocrStatsProcess_t* energyCreateStatsProcess(ocrStats_t *self, ocrGuid_t processGuid) {

    ocrStatsEnergy_t *rself = (ocrStatsEnergy_t*)self;
    ocrStatsProcess_t *result = intCreateStatsProcess(processGuid);

    // Goes here because cannot go in the initialization as PD is not
    // brought up at that point
    if(!rself->aggregatingFilter) {
        rself->aggregatingFilter = newStatsFilter(STATS_FILTER_FILE_DUMP,
                                                  /*parent */NULL, /*param*/NULL);
    }

    /*ocrStatsFilter_t *t = newStatsFilter(STATS_FILTER_ENERGY, rself->aggregatingFilter, NULL);*/
    ocrStatsFilter_t *t2 = newStatsFilter(STATS_FILTER_ENERGY, rself->aggregatingFilter, NULL); // Create different
    // filters since each filter should only be attached to one thing
    // Will print out everything that goes in or out of the object
    /*intStatsProcessRegisterFilter(result, STATS_ALL, t, 0);*/
    intStatsProcessRegisterFilter(result, STATS_ALL, t2, 1);

    DPRINTF(DEBUG_LVL_INFO, "Stats 0x%lx: Created a StatsProcess (0x%lx) for object with GUID 0x%lx\n",
            (u64)self, (u64)result, processGuid);

    return result;
}

static void energyDestructStatsProcess(ocrStats_t *self, ocrStatsProcess_t *process) {
    DPRINTF(DEBUG_LVL_INFO, "Destroying StatsProcess 0x%lx (GUID: 0x%lx)\n", (u64)process, process->me);
    intDestructStatsProcess(process);
}

static ocrStatsFilter_t* energyGetFilter(ocrStats_t *self, ocrStats_t *requester,
                                          ocrStatsFilterType_t type) {

    // Unsupported at this point. This is to support linking filters across policy
    // domains. For now we have one policy domain
    ASSERT(0);
    return NULL;
}

static void energyDumpFilter(ocrStats_t *self, ocrStatsFilter_t *filter, u64 tick, ocrStatsEvt_t type,
                              ocrGuid_t src, ocrGuid_t dest) {

    ASSERT(0); // Don't know what this does
}

// Method to create the default statistics colloector
static ocrStats_t * newStatsEnergy(ocrStatsFactory_t * factory,
                                   ocrParamList_t *perInstance) {

    ocrStatsEnergy_t *result = (ocrStatsEnergy_t*)
        checkedMalloc(result, sizeof(ocrStatsEnergy_t));

    result->aggregatingFilter = NULL;

    result->base.fctPtrs = &(factory->statsFcts);

    return (ocrStats_t*)result;
}

/******************************************************/
/* OCR STATISTICS DEFAULT FACTORY                     */
/******************************************************/

static void destructStatsFactoryEnergy(ocrStatsFactory_t * factory) {
    free(factory);
}

ocrStatsFactory_t * newStatsFactoryEnergy(ocrParamList_t *perType) {
    ocrStatsFactory_t* base = (ocrStatsFactory_t*)
        checkedMalloc(base, sizeof(ocrStatsFactoryEnergy_t));
    base->instantiate = newStatsEnergy;
    base->destruct =  &destructStatsFactoryEnergy;
    base->statsFcts.destruct = &energyDestruct;
    base->statsFcts.setContainingPD = &energySetPD;
    base->statsFcts.createMessage = &energyCreateMessage;
    base->statsFcts.destructMessage = &energyDestructMessage;
    base->statsFcts.createStatsProcess = &energyCreateStatsProcess;
    base->statsFcts.destructStatsProcess = &energyDestructStatsProcess;
    base->statsFcts.getFilter = &energyGetFilter;
    base->statsFcts.dumpFilter = &energyDumpFilter;
    return base;
}

#endif /* OCR_ENABLE_STATISTICS */
