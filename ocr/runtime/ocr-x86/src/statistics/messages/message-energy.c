/**
 * @brief A trivial message that logs just the source, destination and
 * event type
 */

/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifdef OCR_ENABLE_STATISTICS

#include "debug.h"
#include "ocr-guid-kind.h"
#include "message-energy.h"
#include "messages-macros.h"

#include <stdlib.h>
#include <sys/time.h>

#define MESSAGE_NAME ENERGY

// Returns the energy used to transfer the DB.
// TODO: When multiple cache levels are implemented, move the DB as appropriate.
u64 transfer_db(ocrDataBlock_t *db)
{
    // TODO: Do this more intelligently.
    // Current estimate is 2 pJ per bit for DRAM transfer.
    return db->size * 2;
}

MESSAGE_DESTRUCT {
    free(self);
}

MESSAGE_DUMP {
    return NULL;
}

MESSAGE_CREATE {
    MESSAGE_MALLOC(rself);
    MESSAGE_SETUP(rself);

    struct timeval tv;
    gettimeofday(&tv, NULL);
    u64 usecs = tv.tv_sec * 1000000 + tv.tv_usec;
    rself->time_usecs = usecs;

    ocrPolicyDomain_t* policy = getCurrentPD();

    if (type == STATS_EDT_START || type == STATS_EDT_END)
    {
        ocrWorker_t* worker;
        deguidify(policy, src, (u64*)&worker, NULL);
        ocrTask_t* task;
        deguidify(policy, dest, (u64*)&task, NULL);
        ocrTaskTemplate_t* template;
        deguidify(policy, task->templateGuid, (u64*)&template, NULL);

        rself->worker = worker->computes[0]->guid;
        rself->edt_name = template->name;
        rself->edt_guid = task->guid;
        rself->db_size = -1;
        rself->db_guid = NULL_GUID;
        rself->energy_pJ = 0; // TODO: Allow user to specify EDT energy usage.
    }
    else if (type == STATS_DB_ACQ || type == STATS_DB_REL)
    {
        ocrTask_t* task;
        deguidify(policy, src, (u64*)&task, NULL);
        ocrTaskTemplate_t* template;
        deguidify(policy, task->templateGuid, (u64*)&template, NULL);
        ocrDataBlock_t* db;
        deguidify(policy, dest, (u64*)&db, NULL);

        rself->worker = NULL_GUID;
        rself->edt_name = template->name;
        rself->edt_guid = task->guid;
        rself->db_size = db->size;
        rself->db_guid = db->guid;
        rself->energy_pJ = transfer_db(db);
    }

    return (ocrStatsMessage_t*)rself;
}

#undef MESSAGE_NAME

#endif /* OCR_ENABLE_STATISTICS */
