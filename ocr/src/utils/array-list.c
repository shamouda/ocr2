/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-config.h"

#include "ocr-hal.h"
#include "debug.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "utils/array-list.h"

#define DEBUG_TYPE UTIL

#define ARRAY_CHUNK 64

// Implementation of single and double linked list with fixed sized elements.
// Nodes are allocated within array chunks and the node pool is managed by the implementation.
// NOTE: This is *NOT* a concurrent list

typedef struct _arrayChunkNode_t {
    struct _arrayChunkNode_t * next;
} arrayChunkNode_t;

static void newArrayChunkSingle(arrayList_t *list) {
    u32 i;
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    arrayChunkNode_t *chunkNode = (arrayChunkNode_t*) pd->fcts.pdMalloc(pd, sizeof(arrayChunkNode_t) + ((sizeof(slistNode_t) + list->elSize) * list->arrayChunkSize));
    chunkNode->next = list->poolHead;
    list->poolHead = chunkNode;

    u64 ptr = (u64)chunkNode + sizeof(arrayChunkNode_t);
    slistNode_t *headNode = (slistNode_t*)ptr;
    slistNode_t *curNode = headNode;
    for (i = 0; i < list->arrayChunkSize; i++) {
        curNode = (slistNode_t*)ptr;
        curNode->data = (void*)(ptr + sizeof(slistNode_t));
        ptr += sizeof(slistNode_t) + list->elSize;
        curNode->next = (slistNode_t*)ptr;
    }
    curNode->next = list->freeHead;
    list->freeHead = headNode;
}

static void newArrayChunkDouble(arrayList_t *list) {
    u32 i;
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    arrayChunkNode_t *chunkNode = (arrayChunkNode_t*) pd->fcts.pdMalloc(pd, sizeof(arrayChunkNode_t) + ((sizeof(dlistNode_t) + list->elSize) * list->arrayChunkSize));
    chunkNode->next = list->poolHead;
    list->poolHead = chunkNode;

    u64 ptr = (u64)chunkNode + sizeof(arrayChunkNode_t);
    slistNode_t *headNode = (slistNode_t*)ptr;
    slistNode_t *curNode = headNode;
    for (i = 0; i < list->arrayChunkSize; i++) {
        curNode = (slistNode_t*)ptr;
        curNode->data = (void*)(ptr + sizeof(dlistNode_t));
        ((dlistNode_t*)curNode)->prev = NULL;
        ptr += sizeof(dlistNode_t) + list->elSize;
        curNode->next = (slistNode_t*)ptr;
    }
    curNode->next = list->freeHead;
    list->freeHead = headNode;
}

static void newArrayChunk(arrayList_t *list) {
    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        return newArrayChunkSingle(list);
    case OCR_LIST_TYPE_DOUBLE:
        return newArrayChunkDouble(list);
    default:
        ASSERT(0);
        break;
    }
}

/*****************************************************************************/
/* INSERT (BEFORE)                                                           */
/*****************************************************************************/
static void insertArrayListNodeBeforeSingle(arrayList_t *list, slistNode_t *node, slistNode_t *newNode) {
    newNode->next = node;
    if (list->head == node) {
        list->head = newNode;
        if (node == NULL) {
            ASSERT(list->tail == NULL);
            list->tail = newNode;
        }
    } else {
        slistNode_t *last = list->head;
        while (last && last->next != node) last = last->next;
        ASSERT(last);
        last->next = newNode;
    }
    list->count++;
}

static void insertArrayListNodeBeforeDouble(arrayList_t *list, slistNode_t *node, slistNode_t *newNode) {
    dlistNode_t *dNode = (dlistNode_t*)node;
    dlistNode_t *dNewNode = (dlistNode_t*)newNode;
    if (node) {
        newNode->next = node;
        dNewNode->prev = dNode->prev;
        dNode->prev = newNode;
        if (dNewNode->prev) dNewNode->prev->next = newNode;
    } else {
        ASSERT(list->head == NULL);
        ASSERT(list->tail == NULL);
        newNode->next = NULL;
        dNewNode->prev = NULL;
        list->tail = newNode;
    }
    if (list->head == node)
        list->head = newNode;
    list->count++;
}

/*****************************************************************************/
/* INSERT (AFTER)                                                            */
/*****************************************************************************/
static void insertArrayListNodeAfterSingle(arrayList_t *list, slistNode_t *node, slistNode_t *newNode) {
    if (node) {
        newNode->next = node->next;
        node->next = newNode;
    } else {
        ASSERT(list->head == NULL);
        ASSERT(list->tail == NULL);
        newNode->next = NULL;
        list->head = newNode;
    }
    if (list->tail == node)
        list->tail = newNode;
    list->count++;
}

static void insertArrayListNodeAfterDouble(arrayList_t *list, slistNode_t *node, slistNode_t *newNode) {
    dlistNode_t *dNewNode = (dlistNode_t*)newNode;
    if (node) {
        newNode->next = node->next;
        dNewNode->prev = node;
        node->next = newNode;
        if (newNode->next) ((dlistNode_t*)(newNode->next))->prev = newNode;
    } else {
        ASSERT(list->head == NULL);
        ASSERT(list->tail == NULL);
        newNode->next = NULL;
        dNewNode->prev = NULL;
        list->head = newNode;
    }
    if (list->tail == node)
        list->tail = newNode;
    list->count++;
}

/*****************************************************************************/
/* REMOVE                                                                    */
/*****************************************************************************/
static void removeArrayListNodeSingle(arrayList_t *list, slistNode_t *node) {
    ASSERT(node);
    if (list->head == node) {
        list->head = node->next;
        if (list->tail == node) {
            ASSERT(list->head == NULL);
            list->tail = NULL;
        }
    } else {
        slistNode_t *last = list->head;
        while (last && last->next != node) last = last->next;
        ASSERT(last);
        last->next = node->next;
        if (list->tail == node) list->tail = last;
    }
    node->next = NULL;
    list->count--;
}

static void removeArrayListNodeDouble(arrayList_t *list, slistNode_t *node) {
    ASSERT(node);
    dlistNode_t *dNode = (dlistNode_t*)node;
    if (dNode->prev) dNode->prev->next = node->next;
    if (node->next) ((dlistNode_t*)(node->next))->prev = dNode->prev;
    if (list->head == node) list->head = node->next;
    if (list->tail == node) list->tail = dNode->prev;
    node->next = NULL;
    dNode->prev = NULL;
    list->count--;
}

/*****************************************************************************/
/* MOVE (BEFORE)                                                             */
/*****************************************************************************/
static void moveArrayListNodeBeforeSingle(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    ASSERT(src && dst);
    removeArrayListNodeSingle(list, src);
    insertArrayListNodeBeforeSingle(list, dst, src);
}

static void moveArrayListNodeBeforeDouble(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    ASSERT(src && dst);
    removeArrayListNodeDouble(list, src);
    insertArrayListNodeBeforeDouble(list, dst, src);
}

void moveArrayListNodeBefore(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        moveArrayListNodeBeforeSingle(list, src, dst);
        break;
    case OCR_LIST_TYPE_DOUBLE:
        moveArrayListNodeBeforeDouble(list, src, dst);
        break;
    default:
        ASSERT(0);
        break;
    }
}

/*****************************************************************************/
/* MOVE (AFTER)                                                              */
/*****************************************************************************/
static void moveArrayListNodeAfterSingle(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    ASSERT(src && dst);
    removeArrayListNodeSingle(list, src);
    insertArrayListNodeAfterSingle(list, dst, src);
}

static void moveArrayListNodeAfterDouble(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    ASSERT(src && dst);
    removeArrayListNodeDouble(list, src);
    insertArrayListNodeAfterDouble(list, dst, src);
}

void moveArrayListNodeAfter(arrayList_t *list, slistNode_t *src, slistNode_t *dst) {
    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        moveArrayListNodeAfterSingle(list, src, dst);
        break;
    case OCR_LIST_TYPE_DOUBLE:
        moveArrayListNodeAfterDouble(list, src, dst);
        break;
    default:
        ASSERT(0);
        break;
    }
}

/*****************************************************************************/
/* NEW                                                                       */
/*****************************************************************************/
slistNode_t* newArrayListNodeBefore(arrayList_t *list, slistNode_t *node) {
    ASSERT(list->freeHead);
    slistNode_t *newNode = list->freeHead;
    list->freeHead = list->freeHead->next;

    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        insertArrayListNodeBeforeSingle(list, node, newNode);
        break;
    case OCR_LIST_TYPE_DOUBLE:
        insertArrayListNodeBeforeDouble(list, node, newNode);
        break;
    default:
        ASSERT(0);
        break;
    }

    if (list->freeHead == NULL)
        newArrayChunk(list);
    return newNode;
}

slistNode_t* newArrayListNodeAfter(arrayList_t *list, slistNode_t *node) {
    ASSERT(list->freeHead);
    slistNode_t *newNode = list->freeHead;
    list->freeHead = list->freeHead->next;

    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        insertArrayListNodeAfterSingle(list, node, newNode);
        break;
    case OCR_LIST_TYPE_DOUBLE:
        insertArrayListNodeAfterDouble(list, node, newNode);
        break;
    default:
        ASSERT(0);
        break;
    }

    if (list->freeHead == NULL)
        newArrayChunk(list);
    return newNode;
}

/*****************************************************************************/
/* FREE                                                                      */
/*****************************************************************************/
void freeArrayListNodeSingle(arrayList_t *list, slistNode_t *node) {
    ASSERT(node);
    removeArrayListNodeSingle(list, node);
    node->next = list->freeHead;
    list->freeHead = node;
    return;
}

void freeArrayListNodeDouble(arrayList_t *list, slistNode_t *node) {
    ASSERT(node);
    removeArrayListNodeDouble(list, node);
    ((dlistNode_t*)node)->prev = NULL;
    node->next = list->freeHead;
    list->freeHead = node;
    return;
}

void freeArrayListNode(arrayList_t *list, slistNode_t *node) {
    switch(list->type) {
    case OCR_LIST_TYPE_SINGLE:
        return freeArrayListNodeSingle(list, node);
    case OCR_LIST_TYPE_DOUBLE:
        return freeArrayListNodeDouble(list, node);
    default:
        ASSERT(0);
        break;
    }
}

/*****************************************************************************/
/* DESTRUCT LIST                                                             */
/*****************************************************************************/
void destructArrayList(arrayList_t *list) {
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    arrayChunkNode_t *chunkNode = list->poolHead;
    while(chunkNode) {
        arrayChunkNode_t *chunkNodePtr = chunkNode;
        chunkNode = chunkNode->next;
        pd->fcts.pdFree(pd, chunkNodePtr);
    }
    pd->fcts.pdFree(pd, list);
}

/*****************************************************************************/
/* CREATE LIST                                                               */
/*****************************************************************************/
arrayList_t* newArrayList(u32 elSize, u32 arrayChunkSize, ocrListType type) {
    ocrPolicyDomain_t * pd = NULL;
    getCurrentEnv(&pd, NULL, NULL, NULL);
    if (arrayChunkSize == 0) arrayChunkSize = ARRAY_CHUNK;
    arrayList_t* list = (arrayList_t*) pd->fcts.pdMalloc(pd, sizeof(arrayList_t));
    list->type = type;
    list->elSize = elSize;
    list->arrayChunkSize = arrayChunkSize;
    list->poolHead = NULL;
    list->freeHead = NULL;
    list->head = NULL;
    list->tail = NULL;
    list->count = 0;
    newArrayChunk(list);
    return list;
}

/*****************************************************************************/
/* WRAPPER FUNCTIONS                                                         */
/*****************************************************************************/
void* pushFrontArrayList(arrayList_t *list, void *data) {
    slistNode_t *newnode = newArrayListNodeBefore(list, list->head);
    if (data) hal_memCopy(newnode->data, data, list->elSize, 0);
    return newnode->data;
}

void* pushBackArrayList(arrayList_t *list, void *data) {
    slistNode_t *newnode = newArrayListNodeAfter(list, list->tail);
    if (data) hal_memCopy(newnode->data, data, list->elSize, 0);
    return newnode->data;
}

void popFrontArrayList(arrayList_t *list) {
    freeArrayListNode(list, list->head);
}

void popBackArrayList(arrayList_t *list) {
    freeArrayListNode(list, list->tail);
}

void* peekFrontArrayList(arrayList_t *list) {
    return list->head->data;
}

void* peekBackArrayList(arrayList_t *list) {
    return list->tail->data;
}

