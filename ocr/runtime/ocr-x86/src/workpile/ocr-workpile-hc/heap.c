/* Copyright (c) 2012, Rice University

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are
   met:

   1.  Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
   2.  Redistributions in binary form must reproduce the above
   copyright notice, this list of conditions and the following
   disclaimer in the documentation and/or other materials provided
   with the distribution.
   3.  Neither the name of Intel Corporation
   nor the names of its contributors may be used to endorse or
   promote products derived from this software without specific
   prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

#include <stdlib.h>
#include <assert.h>

#include "heap.h"
#include "hc_sysdep.h"
#include "ocr-types.h"
#include "hc_edf.h"
#include "datablock/regular/regular.h"
#include "hc.h"

#include "stdio.h"

void heap_init(heap_t * heap, void * init_value) {
    heap->lock = 0;
    heap->head = 0;
    heap->tail = 0;
    heap->buffer = (buffer_t *) malloc(sizeof(buffer_t));
    heap->buffer->capacity = INIT_HEAP_CAPACITY;
    heap->buffer->data = (volatile void **) malloc(sizeof(void*)*INIT_HEAP_CAPACITY);
    volatile void ** data = heap->buffer->data;
    int i=0;
    while(i < INIT_HEAP_CAPACITY) {
        data[i] = init_value;
        ++i;
    }
}

void locked_heap_tail_push_no_priority (heap_t* heap, void* entry) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;
            int size = heap->tail - heap->head;
            if (size == heap->buffer->capacity) { 
                assert("heap full, increase heap size" && 0);
            }
            int index = heap->tail % heap->buffer->capacity;
            heap->buffer->data[index] = entry;
#ifdef __powerpc64__
            hc_mfence();
#endif 
            ++heap->tail;
            heap->lock = 0;
        }
    }
}

#define MISS 1000
#define REMOTE_L3_HIT 100
#define LOCAL_L3_HIT 50
#define LOCAL_L2_HIT 10

static inline double costToAcquire ( int workerId, u64 placeTracker ) {
    double cost = 0;
    if (!( placeTracker & (1ULL << (16+workerId/8)) )) {
        /*not in my L3, check if it is in neighbour L3*/
        if (!( placeTracker & (1ULL << (16+(1-workerId/8))) )) {
            cost += REMOTE_L3_HIT; 
        } else cost += MISS;
    } else {
        /*in my L3*/
        if ( placeTracker & (1ULL << workerId) ) {
            /*in my L2*/
            cost += LOCAL_L2_HIT;
        } else {
            cost += LOCAL_L3_HIT;
        }
    }
    return cost;
}

static void setCost ( volatile void* entry, int wid ) {
    double totalCost = 0;
    hc_task_t* derived = (hc_task_t*)entry;

    ocrDataBlockPlaced_t *placedDb = NULL;
    ocrGuid_t dbGuid = UNINITIALIZED_GUID;

    int i = 0;
    ocr_event_t* curr = derived->awaitList->array[0];
    while ( NULL != curr ) {
        dbGuid = curr->get(curr);
        if(dbGuid != NULL_GUID) {
            globalGuidProvider->getVal(globalGuidProvider, dbGuid, (u64*)&placedDb, NULL);
            u64 placeTracker = placedDb->placeTracker.existInPlaces;
            totalCost += costToAcquire(wid, placeTracker);
        }

        curr = derived->awaitList->array[++i];
    };
    derived->cost = totalCost;
}

static double extractCost ( volatile void* entry ) {
    /* TODO implement cost extraction*/
    hc_task_t* derived = (hc_task_t*)entry;
    return derived->cost;
}

static inline void swapAtIndices ( heap_t* heap, int firstIndex, int secondIndex ) {
    volatile void* swapTemp = heap->buffer->data[firstIndex];
    heap->buffer->data[firstIndex] = heap->buffer->data[secondIndex];
    heap->buffer->data[secondIndex] = swapTemp;
}

void verifyHeapHelper ( heap_t* heap, int nElements, int firstElementIndex, int checkIndex ) {
    int currHeapIndex = (checkIndex + firstElementIndex) % heap->buffer->capacity;
    double currCost = ((hc_task_t*)(heap->buffer->data[ currHeapIndex ]))->cost;

    if ( checkIndex*2+1 < nElements ) {
        int currLeftHeapIndex = (checkIndex*2+1 + firstElementIndex) % heap->buffer->capacity;
        double leftCost = extractCost(heap->buffer->data[currLeftHeapIndex]);
        assert ( leftCost >= currCost && "broken heap");
        verifyHeapHelper(heap, nElements, firstElementIndex, checkIndex*2+1);

        if ( checkIndex*2+2 < nElements ) {
            int currRightHeapIndex = (checkIndex*2+2 + firstElementIndex) % heap->buffer->capacity;
            double rightCost = extractCost(heap->buffer->data[currRightHeapIndex]);
            assert ( rightCost >= currCost && "broken heap");
            verifyHeapHelper(heap, nElements, firstElementIndex, checkIndex*2+2);
        }
    }
}

static void verifyHeap ( heap_t* heap ) {
    int firstElementIndex = heap->head % heap->buffer->capacity;
    int nElements = heap->tail - heap->head;

    verifyHeapHelper(heap, nElements, firstElementIndex, 0);
}

static void removeFixHeap ( heap_t* heap ) {
    /* set last leaf as root, as the popper already cached the extracted the root task*/
    heap->buffer->data[ ( heap->head ) % heap->buffer->capacity ] = heap->buffer->data[ ( heap->tail - 1 ) % heap->buffer->capacity ];

    /* one less element as root is extracted */
    --heap->tail;


    int firstElementIndex = heap->head % heap->buffer->capacity;
    int nElements = heap->tail - heap->head;

    int heapFixIndex = 0;
    while ( heapFixIndex < nElements ) { //TODO 
        int currHeapIndex = (heapFixIndex + firstElementIndex) % heap->buffer->capacity;
        double currCost = extractCost(heap->buffer->data[currHeapIndex]);

        int heapLeftChildIndex = heapFixIndex*2+1;
        if ( heapLeftChildIndex < nElements ) {
            int currLeftHeapIndex = (heapLeftChildIndex + firstElementIndex) % heap->buffer->capacity;
            double leftCost = extractCost(heap->buffer->data[currLeftHeapIndex]);

            int heapRightChildIndex = heapFixIndex*2+2;

            if ( heapRightChildIndex < nElements ) {
                /*has left and right children */
                int currRightHeapIndex = (heapRightChildIndex + firstElementIndex) % heap->buffer->capacity;
                double rightCost = extractCost(heap->buffer->data[currRightHeapIndex]);

                if ( leftCost < rightCost ) {
                    if ( leftCost < currCost ) {
                        swapAtIndices(heap, currLeftHeapIndex, currHeapIndex);
                        heapFixIndex = heapLeftChildIndex;
                    } else {
                        /*nothing to fix; break the loop*/
                        heapFixIndex = nElements;
                    }
                } else {
                    if ( rightCost < currCost ) {
                        swapAtIndices(heap, currRightHeapIndex, currHeapIndex);
                        heapFixIndex = heapRightChildIndex;
                    } else {
                        /*nothing to fix; break the loop*/
                        heapFixIndex = nElements;
                    }
                }
            } else {
                /*has only left child */
                if ( leftCost < currCost ) {
                    swapAtIndices(heap, currLeftHeapIndex, currHeapIndex);
                    heapFixIndex = heapLeftChildIndex;
                }
            }
        } else {
            /*no left child, nothing to fix; break the loop*/
            heapFixIndex = nElements;
        }
    }

}

static void insertFixHeap ( heap_t* heap ) {
    int firstElementIndex = heap->head % heap->buffer->capacity;

    /*as if heap was 0 indexed, just undo the offsetting caused by the firstElementIndex*/
    int heapFixIndex = heap->tail - 1 - heap->head;

    while( heapFixIndex > 0 ) {
        int currHeapIndex = (heapFixIndex + firstElementIndex) % heap->buffer->capacity;
        double currCost = extractCost(heap->buffer->data[ currHeapIndex ]);

        int parentHeapIndex = ((heapFixIndex-1)/2 + firstElementIndex) % heap->buffer->capacity ;
        double parentCost = extractCost(heap->buffer->data[parentHeapIndex]);

        if ( currCost < parentCost ) {
            swapAtIndices(heap, currHeapIndex, parentHeapIndex);
            /* now fix the new moved level on the tree */
            heapFixIndex = (heapFixIndex-1)/2;
        } else {
            /* nothing to fix, break the loop */
            heapFixIndex = 0;
        }
    }
}

void locked_heap_push_priority (heap_t* heap, void* entry) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;
            int size = heap->tail - heap->head;
            if (size == heap->buffer->capacity) { 
                assert("heap full, increase heap size" && 0);
            }
            int index = heap->tail % heap->buffer->capacity;
            heap->buffer->data[index] = entry;
#ifdef __powerpc64__
            hc_mfence();
#endif 
            ++heap->tail;

            ocrGuid_t worker_guid = ocr_get_current_worker_guid();
            ocr_worker_t * worker = NULL;
            globalGuidProvider->getVal(globalGuidProvider, worker_guid, (u64*)&worker, NULL);
            hc_worker_t * hcWorker = (hc_worker_t *) worker;

            setCost(entry, hcWorker->id);

            insertFixHeap(heap);
            verifyHeap(heap);

            heap->lock = 0;
        }
    }
}

void* locked_heap_pop_priority_best ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size <= 0 ) {
                heap->tail = heap->head;
                rt = NULL;
            } else {
                rt = (void*) heap->buffer->data[ heap->head % heap->buffer->capacity];
                removeFixHeap(heap);
                verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_worst ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size <= 0 ) {
                heap->tail = heap->head;
                rt = NULL;
            } else { 
                --heap->tail;
                rt = (void*) heap->buffer->data[heap->tail % heap->buffer->capacity];
                verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_tail_pop_no_priority ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int tail = heap->tail;
            heap->tail = --tail;
            rt = (void*) heap->buffer->data[tail % heap->buffer->capacity];

            int size = heap->tail - heap->head;
            if ( size < 0 ) {
                heap->tail = heap->head;
                rt = NULL;
            } 
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_head_pop_no_priority ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int head = heap->head;
            heap->head= 1 + head;
            rt = (void*) heap->buffer->data[head % heap->buffer->capacity];

            int size = heap->tail - heap->head;
            if ( size < 0 ) {
                heap->head = heap->tail;
                rt = NULL;
            } 
            heap->lock = 0;
        }
    }
    return rt;
}
