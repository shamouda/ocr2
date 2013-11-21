/*
 * This file is subject to the license agreement located in the file LICENSE
 * and cannot be distributed without it. This notice cannot be
 * removed or modified.
 */

#include "ocr-policy-domain-getter.h"
#include "ocr-policy-domain.h"
#include "ocr-types.h"
#include "ocr-worker.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

ocrGuid_t getCurrentEDTFromWorker() {
    ocrPolicyCtx_t * ctx = getCurrentWorkerContext();
    ocrGuid_t workerGuid = ctx->sourceObj;
    ocrWorker_t *worker = NULL;
    deguidify(ctx->PD, workerGuid, (u64*)&(worker), NULL);
    return worker->fctPtrs->getCurrentEDT(worker);
}

void setCurrentEDTToWorker(ocrGuid_t edtGuid) {
    ocrPolicyCtx_t * ctx = getCurrentWorkerContext();
    ocrGuid_t workerGuid = ctx->sourceObj;
    ocrWorker_t *worker = NULL;
    deguidify(ctx->PD, workerGuid, (u64*)&(worker), NULL);
    worker->fctPtrs->setCurrentEDT(worker, edtGuid);
}

ocrWorker_t *getCurrentWorker()
{
    ocrPolicyCtx_t * ctx = getCurrentWorkerContext();
    ocrGuid_t workerGuid = ctx->sourceObj;
    ocrWorker_t *worker = NULL;
    deguidify(ctx->PD, workerGuid, (u64*)&(worker), NULL);
    return worker;
}

static u64 get_time()
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return t.tv_sec*1000000000 + t.tv_nsec;
}

double get_movement_energy(ocrDataBlockList_t *src, ocrDataBlockList_t *dst, ocrDataBlock_t *db)
{
    ocrPolicyDomain_t *pd = getCurrentPD();
    double energy;
    if (src == dst)
    {
        energy = 0.0;
    }
    else if ((src == &pd->db_list) || (dst == &pd->db_list)) /* Moving to/from DRAM */
    {
        energy = db->size * 8 * 2;  /* Currently hardcoded as 2 pJ/bit */
    }
    else /* Moving between two XEs */
    {
        energy = db->size * 8 * 0.625 * 0.13;  /* Currently hardcoded as 0.13 pJ/bit/mm, 0.625 mm distance */
    }

    return energy;
}


static void dblist_insert(ocrDataBlockList_t *list, ocrDataBlock_t *db)
{
    /*printf("%p: adding db %p to list %p\n", getCurrentWorker(), db, list);*/
    ocrDataBlockNode_t *node = malloc(sizeof(*node));
    node->db = db;
    node->next = list->head;
    list->head = node;
}

static void dblist_remove(ocrDataBlockList_t *list, ocrDataBlock_t *db)
{
    /*printf("%p: removing db %p from list %p\n", getCurrentWorker(), db, list);*/
    if (list->head == NULL)
    {
        goto err;
    }
    else if (list->head->db == db)
    {
        ocrDataBlockNode_t *node = list->head;
        list->head = node->next;
        free(node);
        return;
    }
    else
    {
        ocrDataBlockNode_t *cur = list->head->next;
        ocrDataBlockNode_t *prev = list->head;

        while (cur != NULL)
        {
            if (cur->db == db)
            {
                ocrDataBlockNode_t *temp = cur;
                prev->next = cur->next;
                free(temp);
                return;
            }
            prev = cur;
            cur = cur->next;

        }
    }

err:
    printf("Error from %p: Data block %p does not exist in list %p. Aborting.\n", getCurrentWorker(), db, list);
    exit(1);
}

pthread_mutex_t db_lock = PTHREAD_MUTEX_INITIALIZER;

void storeDB(ocrDataBlockList_t *list, ocrDataBlock_t *db)
{
    if (db->size > list->total_size)
    {
        printf("Error: Data block is too big for local memory. Aborting.\n");
        exit(1);
    }

    ocrWorker_t *worker = getCurrentWorker();
    ocrGuid_t edt = worker->fctPtrs->getCurrentEDT(worker);
    pthread_mutex_lock(&db_lock);
    while (list->used_size + db->size > list->total_size)
    {
        evictLRU(edt, list);
        /*printf("ASDF %#lx\n", edt);*/
    }
    list->used_size += db->size;
    db->location = list;
    dblist_insert(list, db);
    db->last_access = get_time();
    pthread_mutex_unlock(&db_lock);
    /*printf("%lu allocate %p %#lx %lu\n", get_time(), getCurrentWorker(), db->guid, db->size);*/
    /*printf("%p: db = %p, list = %p, size = %lu, used_size = %lu\n", getCurrentWorker(), db, list, db->size, list->used_size);*/
}

ocrDataBlock_t *getLRU(ocrDataBlockList_t *list)
{
    ocrDataBlockNode_t *temp = list->head;
    ocrDataBlock_t *lru = NULL;
    while (temp != NULL)
    {
        ocrDataBlock_t *db = temp->db;
        if ((lru == NULL) || (db->last_access < lru->last_access))
        {
            lru = db;
        }
        temp = temp->next;
    }

    return lru;
}

void evictLRU(ocrGuid_t edt, ocrDataBlockList_t *list)
{
    ocrDataBlock_t *lru = getLRU(list);
    ocrPolicyDomain_t *pd = getCurrentPD();
    /*printf("%p evicting db %p\n", getCurrentWorker(), lru);*/
    moveDB(edt, &pd->db_list, lru);
}

void moveDB(ocrGuid_t edt, ocrDataBlockList_t *dst, ocrDataBlock_t *db)
{
    pthread_mutex_lock(&db_lock);
    if (dst == NULL)
    {
        ocrDataBlockList_t *src = db->location;
        src->used_size -= db->size;
        dblist_remove(src, db);
    }
    if (db->location != dst)
    {
        ocrDataBlockList_t *src = db->location;
        /*printf("%p: moving db %p from list %p to list %p\n", getCurrentWorker(), db, src, dst);*/
        src->used_size -= db->size;
        dblist_remove(src, db);
        while (dst->used_size + db->size > dst->total_size)
        {
            evictLRU(edt, dst);
        }
        dst->used_size += db->size;
        dblist_insert(dst, db);
        db->location = dst;
        double energy = get_movement_energy(src, dst, db);
        char * type = (dst == &getCurrentPD()->db_list) ? "evict" : "acquire";
        printf("%lu DB %s: EDT_NAME (%#lx) -> %#lx (%lu), %f\n", get_time(), type, edt, db->guid, db->size, energy);
    }
    db->last_access = get_time();
    pthread_mutex_unlock(&db_lock);
}

void deleteDB(ocrDataBlock_t *db)
{
    /*printf("%lu free %p %#lx\n", get_time(), getCurrentWorker(), db->guid);*/
    moveDB(0, NULL, db);
}

//void associate_comp_platform_and_worker(ocrWorker_t *worker) {}
