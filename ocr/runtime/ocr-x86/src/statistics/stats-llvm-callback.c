/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <ocr-config.h>

#ifdef OCR_ENABLE_STATISTICS_TEST
#ifdef OCR_ENABLE_PROFILING_STATISTICS_TEST

#include <debug.h>
#include <ocr-runtime-types.h>
#include "ocr-task.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
//#include "ocr-macros.h"
#include "ocr-runtime-itf.h"
#include "ocr-statistics-callbacks.h"
#include "statistics/stats-llvm-callback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

__thread u64 _threadInstructionCount = 0ULL;
__thread u64 _threadFPInstructionCount = 0ULL;
__thread u8 _threadInstrumentOn = 0;

#define MAXEDTS        65536    // TODO: Turn this into a dynamic list
#define ELS_EDT_INDEX  1    // FIXME: Move this someplace global to avoid conflicts
#define MOREDB         8

edtTable_t *edtList[MAXEDTS];
int numEdts = 1;    // 0 is used as placeholder for uninitialized EDTs

volatile int global_mutex = 0;
inline void enter_cs(void) {
    while(!__sync_bool_compare_and_swap(&global_mutex, 0, 1)) {
        sched_yield();
    }
}

inline void leave_cs(void) {
    __sync_fetch_and_and(&global_mutex, 0);
}

    static void ocrStatsAccessInsertDBTable(edtTable_t *edt, ocrDataBlock_t *db, void *addr, u64 len)
    {
        if(edt->maxDbs==edt->numDbs) {
            edt->maxDbs += MOREDB;    // Increase by MOREDB at a time; arbitrary for now
            edt->dbList = (dbTable_t *)realloc(edt->dbList, sizeof(dbTable_t)*edt->maxDbs);
            memset(&edt->dbList[edt->numDbs], 0, (edt->maxDbs-edt->numDbs)*sizeof(dbTable_t));
        }
        edt->dbList[edt->numDbs].db = db;
        edt->dbList[edt->numDbs].start = (u64)addr;
        edt->dbList[edt->numDbs].end = (u64)addr+len;
        edt->dbList[edt->numDbs].readcount = (u64)0;
        edt->dbList[edt->numDbs].readsize = (u64)0;
        edt->dbList[edt->numDbs].writecount = (u64)0;
        edt->dbList[edt->numDbs].writesize = (u64)0;
        edt->numDbs++;
        // printf("Adding DB %p to EDT %p\n", db, edt->task);
    }

    static edtTable_t* ocrStatsAccessInsertEDT(ocrTask_t *task)
    {
        edtTable_t *addedEDT = calloc(1, sizeof(edtTable_t));

        addedEDT->task = task;
        addedEDT->maxDbs = MOREDB;
        addedEDT->dbList = (dbTable_t *)realloc(addedEDT->dbList, addedEDT->maxDbs*sizeof(dbTable_t)); // MOREDB to begin with
        addedEDT->numDbs = 0;

    enter_cs();
    task->els[ELS_EDT_INDEX] = numEdts;
    edtList[numEdts++] = addedEDT;
    leave_cs();

    ASSERT(numEdts<MAXEDTS);  // FIXME: to be made dynamic

    return addedEDT;
}


    void ocrStatsAccessRemoveEDT(ocrTask_t *task)
    {
        s64 i, j;
        edtTable_t *removeEDT;

    if(numEdts==1) return;

    i = (s64)task->els[ELS_EDT_INDEX];
    if(i>0 && edtList[i]->task==task) {  // ignore EDTs that didn't touch DBs
        removeEDT = edtList[i];

        enter_cs();
        edtList[numEdts-1]->task->els[ELS_EDT_INDEX] = i;
        edtList[i] = edtList[--numEdts];
        leave_cs();

            for(j = 0; j<removeEDT->numDbs; j++) {
                printf("EDT %p DB %p size %llx FP %llx Reads %llx Bytes %llx Writes %llx Bytes %llx\n",
                   removeEDT->task->funcPtr, removeEDT->dbList[j].db,
                   removeEDT->dbList[j].end - removeEDT->dbList[j].start, _threadFPInstructionCount,
                   removeEDT->dbList[j].readcount, removeEDT->dbList[j].readsize,
                   removeEDT->dbList[j].writecount, removeEDT->dbList[j].writesize);
            }

            removeEDT->task->els[ELS_EDT_INDEX] = (s64)-1;
            if(removeEDT->dbList) free(removeEDT->dbList);
            free(removeEDT);
        }
    }

void ocrStatsAccessInsertDB(ocrTask_t *task, ocrDataBlock_t *db) {
    s64 i;
    edtTable_t *edt;

    enter_cs();
    i = task->els[ELS_EDT_INDEX];
    if(i>0) {
        edt = edtList[i];
    }
    leave_cs();

        if(db && db->dbType == USER_DBTYPE) {
            ASSERT(db->size!=0);
        
            ocrStatsAccessInsertDBTable(edt, db, db?db->ptr:0, db?db->size:0);
        }
    }

    ocrDataBlock_t* getDBFromAddress(u64 addr, u64 size, u8 iswrite) {
        s64 i;
        u64 j;
        dbTable_t *ptr;
        ocrTask_t *task;
        edtTable_t *edt = NULL;

    deguidify(getCurrentPD(), getCurrentEDT(), (u64*)&task, NULL);
    do {
        i = task->els[ELS_EDT_INDEX];
        if (i == 0) {
            return NULL;
        }
        edt = edtList[i];
    } while(edt->task != task); // To handle slim chance that an EDT insert/remove happened between the 2 lines

        getCurrentEnv(NULL, NULL, &task, NULL);
        do {
            i = task->els[ELS_EDT_INDEX];
            if (i == 0) {
                return NULL;
            }
            edt = edtList[i];
        } while(edt->task != task); // To handle slim chance that an EDT insert/remove happened between the 2 lines

        ptr = edt->dbList;

        for (j = 0; j < edt->numDbs; j++) {
            if((u64)addr >= ptr[j].start && ((u64)addr + size) <= ptr[j].end) {
                if(iswrite) {
                    ptr[j].writecount++; ptr[j].writesize += size;
                } else {
                    ptr[j].readcount++; ptr[j].readsize += size;
                }
                return ptr[j].db;
            }
        }

    for (j = 1; j < edt->numDbs; j++) {
        if((u64)addr >= ptr[j].start && ((u64)addr + size) <= ptr[j].end) {
            return ptr[j].db;
        }
    }

    return NULL; // Returns NULL if address is out of known range (ptr[0] previously used as catch-all but not anymore)
}

    void PROFILER_ocrStatsLoadCallback(void* address, u64 size, u64 count, u64 fpCount) {
       ocrDataBlock_t *db = getDBFromAddress((u64)address, size, 0);
    }

    void PROFILER_ocrStatsStoreCallback(void* address, u64 size, u64 count, u64 fpCount) {
       ocrDataBlock_t *db = getDBFromAddress((u64)address, size, 1);
    }

#ifdef __cplusplus
}
#endif

#endif /* OCR_ENABLE_PROFILING_STATISTICS */
#endif /* OCR_ENABLE_STATISTICS */
