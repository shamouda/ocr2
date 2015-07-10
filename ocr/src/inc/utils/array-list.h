/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef ARRAY_LIST_H_
#define ARRAY_LIST_H_

#include "ocr-config.h"
#include "ocr-types.h"

/****************************************************/
/* ARRAY LIST                                       */
/****************************************************/

typedef enum {
    OCR_LIST_TYPE_SINGLE,
    OCR_LIST_TYPE_DOUBLE,
} ocrListType;

typedef struct _slistNode_t {
    void * data;
    struct _slistNode_t * next;
} slistNode_t;

typedef struct _dlistNode_t {
    slistNode_t base;
    struct _slistNode_t * prev;
} dlistNode_t;

struct _arrayChunkNode_t;

typedef struct _arrayList_t {
    ocrListType type;
    u32 elSize, arrayChunkSize;
    struct _arrayChunkNode_t *poolHead;
    slistNode_t *freeHead;
    slistNode_t *head, *tail;
    u64 count;
} arrayList_t;

//Create a new Array List
arrayList_t* newArrayList(u32 elSize, u32 arrayChunkSize, ocrListType type);

//Create a new list node and insert it after/before the given node. Return the new list node.
slistNode_t* newArrayListNodeAfter(struct _arrayList_t *list, slistNode_t *node);
slistNode_t* newArrayListNodeBefore(struct _arrayList_t *list, slistNode_t *node);

//Move a list node (src) to be before or after a given node (dst)
void moveArrayListNodeAfter(struct _arrayList_t *list, slistNode_t *src, slistNode_t *dst);
void moveArrayListNodeBefore(struct _arrayList_t *list, slistNode_t *src, slistNode_t *dst);

//Free the list node
void freeArrayListNode(struct _arrayList_t *list, slistNode_t *node);

//Wrappers
void* pushFrontArrayList(struct _arrayList_t *list, void *data);
void* pushBackArrayList(struct _arrayList_t *list, void *data);
void  popFrontArrayList(arrayList_t *list);
void  popBackArrayList(arrayList_t *list);
void* peekFrontArrayList(arrayList_t *list);
void* peekBackArrayList(arrayList_t *list);

//Destroy the array list
void destructArrayList(struct _arrayList_t *self);

#endif /* ARRAY_LIST_H_ */
