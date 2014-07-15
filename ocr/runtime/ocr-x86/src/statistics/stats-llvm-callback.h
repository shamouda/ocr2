/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include <ocr-config.h>
#ifdef OCR_ENABLE_STATISTICS_TEST
#ifdef OCR_ENABLE_PROFILING_STATISTICS_TEST
#ifndef __STATS_LLVM_CALLBACK_H__
#define __STATS_LLVM_CALLBACK_H__

#include <ocr-runtime-types.h>
#include "debug.h"
#include "ocr-task.h"
#include "ocr-types.h"

typedef struct _dbTable_t {
    ocrDataBlock_t *db;
    u64 start;
    u64 end;
    u64 readcount, readsize, writecount, writesize;
    u8  slot;
} dbTable_t;

typedef struct _edtTable_t {
    ocrTask_t *task;
    dbTable_t *dbList;
    int numDbs;
    int maxDbs;
    u8 maxslot;
} edtTable_t;

void ocrStatsAccessInsertDB(ocrTask_t *task, ocrDataBlock_t *db);
void ocrStatsAccessRemoveEDT(ocrTask_t *task);
void ocrStatsAccessSetDBSlot(ocrTask_t *task, ocrDataBlock_t *db, u8 slot);

extern void PROFILER_ocrStatsLoadCallback(void* address, u64 size, u64 instrCount, u64 fpInstrCount);
extern void PROFILER_ocrStatsStoreCallback(void* address, u64 size, u64 instrCount, u64 fpInstrCount);

#endif /* __STATS_LLVM_CALLBACK_H__ */
#endif /* OCR_ENABLE_PROFILING_STATISTICS */
#endif /* OCR_ENABLE_STATISTICS */
