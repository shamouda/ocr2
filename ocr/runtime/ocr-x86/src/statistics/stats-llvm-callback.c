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
#include "ocr-runtime-itf.h"
#include "ocr-statistics-callbacks.h"
#include "statistics/stats-llvm-callback.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>

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
    volatile int numEdts = 1;    // 0 is used as placeholder for uninitialized EDTs

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
        edt->dbList[edt->numDbs].slot = 100;
        edt->dbList[edt->numDbs].start = (u64)addr;
        edt->dbList[edt->numDbs].end = (u64)addr+len;
        edt->dbList[edt->numDbs].readcount = (u64)0;
        edt->dbList[edt->numDbs].readsize = (u64)0;
        edt->dbList[edt->numDbs].writecount = (u64)0;
        edt->dbList[edt->numDbs].writesize = (u64)0;
        edt->numDbs++;

#ifdef OCR_ENABLE_EDT_PROFILING_AND_PREDICTION
        // Now revise our prediction based on newly added DB
        ocrTask_t *task = edt->task;
        ocrTaskTemplate_t *taskTemp;
        ocrPolicyDomain_t *pd;
        ocrPolicyMsg_t msg;
        u64 newSize = len + (u64)task->els[27];
        double ic, fp, rd, wr, pow;
        int i;
        struct _profileStruct *profile;

        getCurrentEnv(&pd, NULL, NULL, NULL);

#define PD_MSG (&msg)
#define PD_TYPE PD_MSG_GUID_INFO
        msg.type = PD_MSG_GUID_INFO | PD_MSG_REQUEST | PD_MSG_REQ_RESPONSE;
        PD_MSG_FIELD(guid.guid) = task->templateGuid;
        PD_MSG_FIELD(guid.metaDataPtr) = NULL;
        PD_MSG_FIELD(properties) = KIND_GUIDPROP | RMETA_GUIDPROP;
        RESULT_PROPAGATE2(pd->fcts.processMessage(pd, &msg, false), ERROR_GUID);
        taskTemp = (ocrTaskTemplate_t *)PD_MSG_FIELD(guid.metaDataPtr);
#undef PD_MSG
#undef PD_TYPE

        profile = taskTemp->profileData;
        if(profile) {
            ic = 0.0; pow = 1;
            for(i = 0; i<profile->numIcCoeff; i++) {
                ic += pow*profile->icCoeff[i];
                pow *= newSize;
            }
            fp = 0.0; pow = 1;
            for(i = 0; i<profile->numFpCoeff; i++) {
                fp += pow*profile->fpCoeff[i];
                pow *= newSize;
            }
            rd = 0.0; pow = 1;
            for(i = 0; i<profile->numRdCoeff; i++) {
                rd += pow*profile->rdCoeff[i];
                pow *= newSize;
            }
            wr = 0.0; pow = 1;
            for(i = 0; i<profile->numWrCoeff; i++) {
                wr += pow*profile->wrCoeff[i];
                pow *= newSize;
            }
            printf("REVISEDPREDICTION: at %#lx %p %s SZ %llu IC %f FP %f RD %f WR %f\n",
                           _threadInstructionCount,
                           taskTemp->executePtr, taskTemp->profileData->fname,
                           (unsigned long long)newSize, ic, fp, rd, wr);

           task->els[27] = (ocrGuid_t)(newSize);
           /* Save these onto ELS */
           task->els[28] = (ocrGuid_t)(ic);
           task->els[29] = (ocrGuid_t)(fp);
           task->els[30] = (ocrGuid_t)(rd);
           task->els[31] = (ocrGuid_t)(wr);
       }
#endif

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
        ASSERT(addedEDT->task==task);

        return addedEDT;
    }


    void ocrStatsAccessRemoveEDT(ocrTask_t *task)
    {
        s64 i, j;
        u64 index;
        edtTable_t *removeEDT;

        if(numEdts==1) return;

        do {
            index = *(volatile u64 *)&(task->els[ELS_EDT_INDEX]);
            if(index == 0) return;
            removeEDT = edtList[index];
        } while(!removeEDT || removeEDT->task != task); // To handle slim chance that an EDT insert/remove happened between the 2 lines

        enter_cs();
        i = (s64)task->els[ELS_EDT_INDEX];
        if(i!=numEdts-1) {
            edtList[i] = edtList[numEdts-1]; // Move the last element in this EDT's place
            edtList[i]->task->els[ELS_EDT_INDEX] = i; // Update its els index
        }
        edtList[--numEdts] = NULL;
        leave_cs();

        for(j = 0; j<removeEDT->numDbs; j++) {
            printf("EDT %p %d DB %p size %#lx IC %#lx FP %#lx Reads %#lx Bytes %#lx Writes %#lx Bytes %#lx\n",
               removeEDT->task->funcPtr, removeEDT->dbList[j].slot, removeEDT->dbList[j].db,
               removeEDT->dbList[j].end - removeEDT->dbList[j].start,
               _threadInstructionCount, _threadFPInstructionCount,
               removeEDT->dbList[j].readcount, removeEDT->dbList[j].readsize,
               removeEDT->dbList[j].writecount, removeEDT->dbList[j].writesize);
        }

        u64 size, numreads, reads, numwrites, writes;
        s64 IC, FP;

        FP = _threadFPInstructionCount;
        IC = _threadInstructionCount;

        size = numreads = reads = numwrites = writes = 0;
        for(j = 0; j<removeEDT->numDbs; j++) {
            size += (removeEDT->dbList[j].end - removeEDT->dbList[j].start);
            reads += removeEDT->dbList[j].readsize;
            numreads += removeEDT->dbList[j].readcount;
            writes += removeEDT->dbList[j].writesize;
            numwrites += removeEDT->dbList[j].writecount;
        }
        printf("OBSERVED: %p SZ %llu IC %lld FP %lld RD %llu WR %llu\n",
               removeEDT->task->funcPtr, (long long unsigned int)size,
               (long long signed int)IC, (long long signed int)FP,
               (long long unsigned int)reads, (long long unsigned int)writes);

#ifdef OCR_ENABLE_EDT_PROFILING_AND_PREDICTION
        double ic_err, fp_err, rd_err, wr_err;
        ic_err = (double)removeEDT->task->els[28];
        fp_err = (double)removeEDT->task->els[29];
        rd_err = (double)removeEDT->task->els[30];
        wr_err = (double)removeEDT->task->els[31];

        ic_err = (1.0*IC - ic_err);
        fp_err = (1.0*FP - fp_err);
        rd_err = (1.0*reads - rd_err);
        wr_err = (1.0*writes - wr_err);

        if(ic_err != 0) ic_err = 100*ic_err/IC;
        if(fp_err != 0) fp_err = 100*fp_err/FP;
        if(rd_err != 0) rd_err = 100*rd_err/reads;
        if(wr_err != 0) wr_err = 100*wr_err/writes;

        printf("MISPREDICTION %p SZ %llu IC %f FP %f RD %f WR %f\n",
               removeEDT->task->funcPtr, (long long unsigned int)size,
               ic_err, fp_err, rd_err, wr_err);
#endif

        removeEDT->task->els[ELS_EDT_INDEX] = (s64)-1;
        if(removeEDT->dbList) free(removeEDT->dbList);
        if(removeEDT) free((void *)removeEDT);
    }

    void ocrStatsAccessSetDBSlot(ocrTask_t *task, ocrDataBlock_t *db, u8 slot) {
        u64 j;
        dbTable_t *ptr;
        edtTable_t *edt = NULL;

        do {
            j = *(volatile u64 *)&(task->els[ELS_EDT_INDEX]);
            ASSERT(j);
            edt = edtList[j];
        } while(!edt || edt->task != task); // To handle slim chance that an EDT insert/remove happened between the 2 lines

        ptr = edt->dbList;

        for (j = 0; j < edt->numDbs; j++) {
            if(ptr[j].db == db) {
                ptr[j].slot = slot;
                break;
            }
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

        if(i<=0) edt = ocrStatsAccessInsertEDT(task);

        if(db && db->dbType == USER_DBTYPE) {
            ASSERT(db->size!=0);
            ocrStatsAccessInsertDBTable(edt, db, db->ptr, db->size);
        }
    }

    ocrDataBlock_t* getDBFromAddress(u64 addr, u64 size, u8 iswrite) {
        u64 j;
        dbTable_t *ptr;
        ocrTask_t *task;
        edtTable_t *edt = NULL;

        getCurrentEnv(NULL, NULL, &task, NULL);
        do {
            j = *(volatile u64 *)&(task->els[ELS_EDT_INDEX]);
            if(j==0) return NULL;
            edt = edtList[j];
        } while(!edt || edt->task != task); // To handle slim chance that an EDT insert/remove happened between the 2 lines

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

        return NULL; // Returns NULL if address is out of known range (ptr[0] previously used as catch-all but not anymore)
    }

    void PROFILER_ocrStatsLoadCallback(void* address, u64 size, u64 count, u64 fpCount) {
       getDBFromAddress((u64)address, size, 0);
    }

    void PROFILER_ocrStatsStoreCallback(void* address, u64 size, u64 count, u64 fpCount) {
       getDBFromAddress((u64)address, size, 1);
    }

#ifdef __cplusplus
}
#endif

#endif /* OCR_ENABLE_PROFILING_STATISTICS */
#endif /* OCR_ENABLE_STATISTICS */
