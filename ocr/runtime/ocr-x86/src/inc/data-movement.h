/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef DATA_MOVEMENT_H_
#define __DATA_MOVEMENT_H__

void storeDB(ocrDataBlockList_t *list, ocrDataBlock_t *db);
void evictLRU(ocrGuid_t edt, ocrDataBlockList_t *list);
void moveDB(ocrGuid_t edt, ocrDataBlockList_t *dst, ocrDataBlock_t *db);
void deleteDB(ocrDataBlock_t *db);

#endif /* DATA_MOVEMENT_H_ */
