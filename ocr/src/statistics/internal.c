/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

// BUG #225: Implement statistics. This is an older version

#ifdef OCR_ENABLE_STATISTICS
#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-statistics.h"
#include "ocr-types.h"
#include "statistics/internal.h"

#include <stdlib.h>
#include <string.h>

#define DEBUG_TYPE STATS

ocrStatsProcess_t* intCreateStatsProcess(ocrGuid_t processGuid) {
    ocrStatsProcess_t *result = checkedMalloc(result, sizeof(ocrStatsProcess_t));

    u64 i;
    ocrPolicyDomain_t *policy = getCurrentPD();
    ocrPolicyCtx_t *ctx = getCurrentWorkerContext();

    result->me = processGuid;
    result->processing = policy->getLock(policy, ctx);
    result->messages = policy->getQueue(policy, 32, ctx);
    result->tick = 0;

    // +1 because the first bucket keeps track of all filters uniquely
    result->outFilters = (ocrStatsFilter_t***)checkedMalloc(result->outFilters,
                         sizeof(ocrStatsFilter_t**)*(STATS_EVT_MAX+1));
    result->outFilterCounts = (u64*)checkedMalloc(result->outFilterCounts, sizeof(u64)*(STATS_EVT_MAX+1));
    result->filters = (ocrStatsFilter_t***)checkedMalloc(result->filters,
                      sizeof(ocrStatsFilter_t**)*(STATS_EVT_MAX+1));
    result->filterCounts = (u64*)checkedMalloc(result->filterCounts, sizeof(u64)*(STATS_EVT_MAX+1));

    for(i = 0; i < (u64)STATS_EVT_MAX + 1; ++i) {
        result->outFilterCounts[i] = 0ULL;
        result->filterCounts[i] = 0ULL;
    }

    return result;
}

void intDestructStatsProcess(ocrStatsProcess_t *process) {
    u64 i;
    u32 j;


    // Make sure we process all remaining messages
    while(!process->processing->fctPtrs->trylock(process->processing)) ;

    while(intProcessMessage(process)) ;

    // Free all the filters associated with this process
    // Assumes that the filter is only associated with ONE
    // process
    u32 maxJ = (u32)(process->outFilterCounts[0] & 0xFFFFFFFF);
    for(j = 0; j < maxJ; ++j) {
        process->outFilters[0][j]->fcts.destruct(process->outFilters[0][j]);
    }
    if(maxJ)
        free(process->outFilters[0]);
    for(i = 1; i < (u64)STATS_EVT_MAX + 1; ++i) {
        if(process->outFilterCounts[i])
            free(process->outFilters[i]);
    }

    maxJ = (u32)(process->filterCounts[0] & 0xFFFFFFFF);
    for(j = 0; j < maxJ; ++j) {
        process->filters[0][j]->fcts.destruct(process->filters[0][j]);
    }
    if(maxJ)
        free(process->filters[0]);
    for(i = 1; i < (u64)STATS_EVT_MAX + 1; ++i) {
        if(process->filterCounts[i])
            free(process->filters[i]);
    }

    free(process->outFilters);
    free(process->outFilterCounts);
    free(process->filters);
    free(process->filterCounts);

    process->processing->fctPtrs->unlock(process->processing);
    process->processing->fctPtrs->destruct(process->processing);

    process->messages->fctPtrs->destruct(process->messages);

}

void intStatsProcessRegisterFilter(ocrStatsProcess_t *self, u64 bitMask,
                                   ocrStatsFilter_t *filter, u8 out) {

    DPRINTF(DEBUG_LVL_VERB, "Registering %s filter 0x%"PRIx64" with process 0x%"PRIx64" for mask 0x%"PRIx64"\n",
            out?"OUT":"IN", (u64)filter, (u64)self, bitMask);
    u32 countAlloc, countPresent;

    ocrStatsFilter_t ****filterVar = NULL;
    u64 **countVar = NULL;
    if(out) {
        filterVar = &(self->outFilters);
        countVar = &(self->outFilterCounts);
    } else {
        filterVar = &(self->filters);
        countVar = &(self->filterCounts);
    }

    while(bitMask) {
        // Setting some filter up
        u64 bitSet = fls64(bitMask);
        bitMask &= ~(1ULL << bitSet);

        ASSERT(bitSet < STATS_EVT_MAX);
        ++bitSet; // 0 is all filters uniquely

        // Check to make sure we have enough room to add this filter
        countPresent = (u32)((*countVar)[bitSet] & 0xFFFFFFFF);
        countAlloc = (u32)((*countVar)[bitSet] >> 32);
        //DPRINTF(DEBUG_LVL_VVERB, "For type %"PRId64", have %"PRId32" present and %"PRId32" allocated (from 0x%"PRIx64")\n",
        //        bitSet, countPresent, countAlloc, (*countVar)[bitSet]);
        if(countAlloc <= countPresent) {
            // Allocate using an exponential model
            if(countAlloc)
                countAlloc *= 2;
            else
                countAlloc = 1;
            ocrStatsFilter_t **newAlloc = (ocrStatsFilter_t**)malloc(sizeof(ocrStatsFilter_t*)*countAlloc);
            memcpy(newAlloc, (*filterVar)[bitSet], countPresent*sizeof(ocrStatsFilter_t*));
            if(countAlloc != 1)
                free((*filterVar)[bitSet]);
            (*filterVar)[bitSet] = newAlloc;
        }
        (*filterVar)[bitSet][countPresent++] = filter;

        // Set the counter properly again
        (*countVar)[bitSet] = ((u64)countAlloc << 32) | (countPresent);
        //DPRINTF(DEBUG_LVL_VVERB, "Setting final counter for %"PRId64" to 0x%"PRIx64"\n",
        //        bitSet, (*countVar)[bitSet]);
    }
    // Do the same thing for bit 0. Only do it once so we only free things once
    countPresent = (u32)((*countVar)[0] & 0xFFFFFFFF);
    countAlloc = (u32)((*countVar)[0] >> 32);
    if(countAlloc <= countPresent) {
        // Allocate using an exponential model
        if(countAlloc)
            countAlloc *= 2;
        else
            countAlloc = 1;
        ocrStatsFilter_t **newAlloc = (ocrStatsFilter_t**)malloc(sizeof(ocrStatsFilter_t*)*countAlloc);
        memcpy(newAlloc, (*filterVar)[0], countPresent*sizeof(ocrStatsFilter_t*));
        if(countAlloc != 1)
            free((*filterVar)[0]);
        (*filterVar)[0] = newAlloc;
    }
    (*filterVar)[0][countPresent++] = filter;

    // Set the counter properly again
    (*countVar)[0] = ((u64)countAlloc << 32) | (countPresent);

}

u8 intProcessOutgoingMessage(ocrStatsProcess_t *src, ocrStatsMessage_t* msg) {
    DPRINTF(DEBUG_LVL_VERB, "Processing outgoing message 0x%"PRIx64" for "GUIDF"\n",
            (u64)msg, GUIDA(src->me));


    u64 type = fls64(msg->type);
    ASSERT((1ULL<<type) == msg->type);

    u32 countFilter = src->outFilterCounts[type] & 0xFFFFFFFF;
    if(countFilter) {
        // We have at least one filter that is registered to
        // this message type
        ocrStatsFilter_t **myFilters =  src->outFilters[type];
        while(countFilter-- > 0) {
            DPRINTF(DEBUG_LVL_VVERB, "Sending message 0x%"PRIx64" to filter 0x%"PRIx64"\n",
                    (u64)msg, (u64)myFilters[countFilter]);
            myFilters[countFilter]->fcts.notify(myFilters[countFilter], msg);
        }
    }

    return 0;
}

u8 intProcessMessage(ocrStatsProcess_t *dst) {
    ocrStatsMessage_t* msg = (ocrStatsMessage_t*)
                             dst->messages->fctPtrs->popHead(dst->messages);

    if(msg) {
        DPRINTF(DEBUG_LVL_VERB, "Processing incoming message 0x%"PRIx64" for "GUIDF"\n",
                (u64)msg, GUIDA(dst->me));
        u64 newTick = msg->tick > (dst->tick + 1)?msg->tick:(dst->tick + 1);
        msg->tick = dst->tick = newTick;

        u64 type = fls64(msg->type);
        ASSERT((1ULL<<type) == msg->type);

        u32 countFilter = dst->filterCounts[type] & 0xFFFFFFFF;
        if(countFilter) {
            // We have at least one filter that is registered to
            // this message type
            ocrStatsFilter_t **myFilters =  dst->filters[type];
            while(countFilter-- > 0) {
                DPRINTF(DEBUG_LVL_VVERB, "Sending message 0x%"PRIx64" to filter 0x%"PRIx64"\n",
                        (u64)msg, (u64)myFilters[countFilter]);
                myFilters[countFilter]->fcts.notify(myFilters[countFilter], msg);
            }
        }
        if(msg->state == 1) {
            msg->state = 2;
        } else {
            ASSERT(msg->state == 0);
            msg->fcts.destruct(msg);
        }
        return 1;
    } else {
        return 0;
    }
}
#endif /* OCR_ENABLE_STATISTICS */
