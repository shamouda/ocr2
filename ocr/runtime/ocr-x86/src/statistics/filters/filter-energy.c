/**
 * @brief A trivial filter that logs everything that comes in to it
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifdef OCR_ENABLE_STATISTICS

#include "debug.h"
#include "filters-macros.h"
#include "ocr-policy-domain-getter.h"
#include "ocr-policy-domain.h"
#include "statistics/messages/messages-macros.h"
#include "statistics/messages/message-energy.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILTER_NAME ENERGY

// A simple filter (simply stores the events and will re-dump them out)
typedef struct {
    u64 tick;
    ocrGuid_t src, dest;
    ocrGuidKind srcK, destK;
    ocrStatsEvt_t type;
    u64 time_nsecs;
    ocrGuid_t worker;
    const char * edt_name;
    ocrGuid_t edt_guid;
    u64 db_size;
    ocrGuid_t db_guid;
    u64 energy_pJ;
} myMessageNode_t;

START_FILTER
u64 count, maxCount;
myMessageNode_t *messages;
END_FILTER

FILTER_DESTRUCT {
    FILTER_CAST(rself, self);
    DPRINTF(DEBUG_LVL_VERB, "Destroying energy filter @ 0x%lx\n", (u64)self);
    if(self->parent) {
        DPRINTF(DEBUG_LVL_VERB, "Filter @ 0x%lx merging with parent @ 0x%lx\n",
                (u64)self, (u64)(self->parent));
        return self->parent->fcts.merge(self->parent, self, 1);
    } else {
        free(rself->messages);
        free(self);
    }
}

FILTER_DUMP {
    FILTER_CAST(rself, self);

    if(rself->count == 0)
        return 0;

    ASSERT(chunk < rself->count);

    const int max_msg_len = 150;
    *out = (char*)malloc(sizeof(char)*max_msg_len);

    myMessageNode_t *tmess = &(rself->messages[chunk]);
    if (tmess->type == STATS_EDT_START || tmess->type == STATS_EDT_END)
    {
        snprintf(*out, max_msg_len, "%lu EDT %s: %#lx -> %s (%#lx), %lu",
                 tmess->time_nsecs, tmess->type == STATS_EDT_START ? "start" : "end",
                 tmess->worker, tmess->edt_name, tmess->edt_guid, tmess->energy_pJ);
    }
    else if (tmess->type == STATS_DB_ACQ || tmess->type == STATS_DB_REL)
    {
        snprintf(*out, max_msg_len, "%lu DB %s: %s (%#lx) -> %#lx (%lu), %lu",
                 tmess->time_nsecs, tmess->type == STATS_DB_ACQ ? "acquire" : "release",
                 tmess->edt_name, tmess->edt_guid, tmess->db_guid, tmess->db_size, tmess->energy_pJ);
    }

    if(chunk < rself->count - 1)
        return chunk+1;
    return 0;
}

FILTER_NOTIFY {
    FILTER_CAST(rself, self);
    MESSAGE_TYPE_CAST(ENERGY, emess, mess);

    if(rself->count + 1 == rself->maxCount) {
        // Allocate more space
        rself->maxCount *= 2;
        myMessageNode_t *t = (myMessageNode_t*)malloc(
            sizeof(myMessageNode_t)*rself->maxCount);
        memcpy(t, rself->messages, rself->count*sizeof(myMessageNode_t));
        rself->messages = t;
    }

    DPRINTF(DEBUG_LVL_VVERB, "Filter @ 0x%lx received message (%ld): 0x%lx -> 0x%lx of type 0x%x\n",
            (u64)self, rself->count, mess->src, mess->dest, (u32)mess->type);

    myMessageNode_t* tmess = &(rself->messages[rself->count++]);
    tmess->tick = emess->base.tick;
    tmess->src = emess->base.src;
    tmess->srcK = emess->base.srcK;
    tmess->dest = emess->base.dest;
    tmess->destK = emess->base.destK;
    tmess->type = emess->base.type;

    tmess->time_nsecs = emess->time_nsecs;
    tmess->worker = emess->worker;
    tmess->edt_name = emess->edt_name;
    tmess->edt_guid = emess->edt_guid;
    tmess->db_size = emess->db_size;
    tmess->db_guid = emess->db_guid;
    tmess->energy_pJ = emess->energy_pJ;
}

FILTER_MERGE {
    ASSERT(0);
}

FILTER_CREATE {
    FILTER_MALLOC(rself);
    FILTER_SETUP(rself, parent);

    rself->count = 0;
    rself->maxCount = 100; // Make a configurable number
    DPRINTF(DEBUG_LVL_VERB, "Created a simple filter @ 0x%lx with parent 0x%lx\n",
            (u64)rself, (u64)parent);
    rself->messages = (myMessageNode_t*)malloc(sizeof(myMessageNode_t)*rself->maxCount);

    return (ocrStatsFilter_t*)rself;
}

#undef FILTER_NAME

#endif /* OCR_ENABLE_STATISTICS */
