/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#ifndef ARRAY_LIST_H_
#define ARRAY_LIST_H_

#include "ocr-config.h"
#include "ocr-types.h"

// This is a linked-list that is capable of acting as
// either a regular linked list or as a priority list.
// It should be used in either of one of these modes.
// The list can also be configured as either a single
// or double linked list.

// In priority list mode, nodes should be kept sorted
// in ascending order of the node key.
// Ordering checks are made when a new node is inserted.
// So, it is the user's responsibility to insert a new
// node at the right place. A node's priority can be
// updated by calling the updatePriority API.

/****************************************************/
/* ARRAY LIST                                       */
/****************************************************/

typedef enum {
    OCR_LIST_TYPE_SINGLE,
    OCR_LIST_TYPE_DOUBLE,
} ocrListType;

typedef struct _slistNode_t {
    void * data;
    u64 key;            //key used to sort the list in ascending order
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

/*COMMON (REGULAR/PRIORITY) LIST API:*/
//Create a new Array List
arrayList_t* newArrayList(u32 elSize, u32 arrayChunkSize, ocrListType type);

//Destroy the array list
void destructArrayList(struct _arrayList_t *self);

//Free the list node
void freeArrayListNode(struct _arrayList_t *list, slistNode_t *node);

/*REGULAR LIST API:*/
//Create a new list node and insert it after/before the given node. Return the new list node.
slistNode_t* newArrayListNodeAfter(struct _arrayList_t *list, slistNode_t *node);
slistNode_t* newArrayListNodeBefore(struct _arrayList_t *list, slistNode_t *node);

//Move a list node (src) to be before or after a given node (dst)
void moveArrayListNodeAfter(struct _arrayList_t *list, slistNode_t *src, slistNode_t *dst);
void moveArrayListNodeBefore(struct _arrayList_t *list, slistNode_t *src, slistNode_t *dst);

//Wrappers
void* pushFrontArrayList(struct _arrayList_t *list, void *data);
void* pushBackArrayList(struct _arrayList_t *list, void *data);
void  popFrontArrayList(arrayList_t *list);
void  popBackArrayList(arrayList_t *list);
void* peekFrontArrayList(arrayList_t *list);
void* peekBackArrayList(arrayList_t *list);

/*PRIORITY LIST API:*/
//Create priority list node
slistNode_t* newPriorityArrayListNodeAfter(arrayList_t *list, slistNode_t *node, u64 key);
slistNode_t* newPriorityArrayListNodeBefore(arrayList_t *list, slistNode_t *node, u64 key);

//Update priority list node
void updatePriorityArrayListNode(arrayList_t *list, slistNode_t *node, u64 newKey);

#endif /* ARRAY_LIST_H_ */
