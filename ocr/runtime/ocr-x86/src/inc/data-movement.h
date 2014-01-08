/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef __DATA_MOVEMENT_H__
#define __DATA_MOVEMENT_H__

#include "ocr-datablock.h"

#define L1SPAD_SIZE (4*1024) // Workers
#define L2SPAD_SIZE (512*1024) // Block
#define L3SPAD_SIZE (2*1024*1024) // Unit
#define L4SPAD_SIZE (8*1024*1024) // Chip

typedef struct _ocrMemoryBlock_t {
	ocrDataBlockList_t db_list;
	struct _ocrMemoryBlock_t *parent;
} ocrMemoryBlock_t;

void storeDB(ocrDataBlockList_t *list, ocrDataBlock_t *db);
void evictLRU(ocrGuid_t edt, ocrDataBlockList_t *list);
void moveDB(ocrGuid_t edt, ocrDataBlockList_t *dst, ocrDataBlock_t *db);
void deleteDB(ocrDataBlock_t *db);

#endif /* DATA_MOVEMENT_H_ */
