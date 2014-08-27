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

#define DEBUG 0

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

static inline void push_capacity_assertion ( heap_t* heap ) {
    int size = heap->tail - heap->head;
    if (size == heap->buffer->capacity) { 
        assert("heap full, increase heap size" && 0);
    }
}

void locked_heap_tail_push_no_priority (heap_t* heap, void* entry) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;
            push_capacity_assertion(heap);

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
#define LOCAL_L3_HIT 50
#define LOCAL_L2_HIT 10

#define N_SOCKETS 2
#define N_L3S_PER_SOCKET 1
#define N_L2S_PER_SOCKET 6
#define N_L1S_PER_SOCKET 6
#define N_TOTAL_L3S ( (N_SOCKETS) * (N_L3S_PER_SOCKET))
#define N_TOTAL_L2S ( (N_SOCKETS) * (N_L2S_PER_SOCKET))
#define N_TOTAL_L1S ( (N_SOCKETS) * (N_L1S_PER_SOCKET))

#define DAVINCI
static inline double costToAcquire ( int workerId, u64 placeTracker ) {
    double cost = 0;
#ifdef DAVINCI
    unsigned char socket_id = workerId/N_L1S_PER_SOCKET;
    unsigned char l2_within_socket_id = workerId % N_L2S_PER_SOCKET;
    unsigned char l3_within_socket_id = 0;

    if (!( placeTracker & (1ULL << (N_TOTAL_L1S + N_TOTAL_L2S + N_TOTAL_L3S + socket_id)) )) { /*not in my socket*/
        cost += MISS;
    } else if (!( placeTracker & (1ULL << (N_TOTAL_L1S + N_TOTAL_L2S + socket_id*N_L3S_PER_SOCKET + l3_within_socket_id )) )) { /*in socket, not in my L3*/
        cost += MISS;
    } else if (!( placeTracker & (1ULL << (N_TOTAL_L1S + socket_id*N_L2S_PER_SOCKET + l2_within_socket_id)) )) { /*in socket, in L3, not in my L2*/
        cost += LOCAL_L3_HIT;
    } else {
        cost += LOCAL_L2_HIT;
    }
#endif
    return cost;
}

static inline double costToAcquireEvent ( int workerId, int put_cpu_id ) {
    double cost = 0;
#ifdef DAVINCI
    unsigned char worker_socket_id = workerId/N_L2S_PER_SOCKET;
    unsigned char worker_l2_within_socket_id = workerId % N_L2S_PER_SOCKET;

    unsigned char put_socket_id = put_cpu_id/N_L2S_PER_SOCKET;
    unsigned char put_l2_within_socket_id = put_cpu_id % N_L2S_PER_SOCKET;

    if ( put_socket_id != worker_socket_id ) { /*not in my socket*/
        cost += MISS;
    } else if ( put_l2_within_socket_id != worker_l2_within_socket_id ) { /*in socket, in L3, not in my L2*/
        cost += LOCAL_L3_HIT;
    } else {
        cost += LOCAL_L2_HIT;
    }
#endif
    return cost;
}

static double calculateTaskRetrievalCost ( volatile void* entry, int wid ) {
    hc_task_t* derived = (hc_task_t*)entry;

    ocrDataBlockPlaced_t *placedDb = NULL;
    ocrGuid_t dbGuid = UNINITIALIZED_GUID;

    int i = 0;
    double totalCost = 0;

    ocr_event_t** eventArray = derived->awaitList->array;
    ocr_event_t* curr = eventArray[0];

    while ( NULL != curr ) {
        dbGuid = curr->get(curr);

        if(dbGuid != NULL_GUID) {
            globalGuidProvider->getVal(globalGuidProvider, dbGuid, (u64*)&placedDb, NULL);
            u64 placeTracker = placedDb->placeTracker.existInPlaces;
            totalCost += costToAcquire(wid, placeTracker);
        }

        curr = eventArray[++i];
    };

    return totalCost;
}

static double calculateTaskRetrievalCostFromEvents ( volatile void* entry, int wid ) {
    hc_task_t* derived = (hc_task_t*)entry;

    int i = 0;
    double totalCost = 0;

    ocr_event_t** eventArray = derived->awaitList->array;
    hc_event_t* curr = NULL;

    int nEvent = 0;
    for ( curr = (hc_event_t*)eventArray[0]; NULL != curr; curr = (hc_event_t*)eventArray[++nEvent] ) {
        totalCost += costToAcquireEvent (wid, curr->put_cpu_id);
    }
    return totalCost;
}

static void setLocalCost ( volatile void* entry, int wid ) {
    hc_task_t* derived = (hc_task_t*)entry;
    derived->cost = calculateTaskRetrievalCostFromEvents ( entry, wid );
}

static inline double extractCost ( volatile void* entry ) {
    /* TODO implement cost extraction*/
    hc_task_t* derived = (hc_task_t*)entry;
    return derived->cost;
}

/*sagnak awful awful hardcoding*/
/*returns cheapest task for the thief to execute by handing out its "heap" index*/
static int getMostLocal( heap_t* heap, int thiefID ) {
    int currHeapIndex = 0;
    int cheapestHeapIndex = 0;
    int size = heap->tail - heap->head;
    int minCost = 10000000;
    while ( currHeapIndex < size ) {
        volatile void* currTask = heap->buffer->data[(heap->head+currHeapIndex) % heap->buffer->capacity];
        int currCost = calculateTaskRetrievalCost (currTask, thiefID);
        if ( minCost > currCost ) {
            minCost = currCost;
            cheapestHeapIndex = currHeapIndex;
        }
        ++currHeapIndex;
    }
    return cheapestHeapIndex;
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
    /* one less element as root is extracted */
    heap->buffer->data[ ( heap->head ) % heap->buffer->capacity ] = heap->buffer->data[ ( --heap->tail ) % heap->buffer->capacity ];

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
                } else {
                    /*nothing to fix; break the loop*/
                    heapFixIndex = nElements;
                }
            }
        } else {
            /*no left child, nothing to fix; break the loop*/
            heapFixIndex = nElements;
        }
    }

}

static void insertFixHeapAtIndex ( heap_t* heap, int heapFixIndex ) {
    int firstElementIndex = heap->head % heap->buffer->capacity;

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

static void insertFixHeap ( heap_t* heap ) {
    insertFixHeapAtIndex(heap, heap->tail - heap->head - 1);
}

void locked_heap_push_priority (heap_t* heap, void* entry) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;

            push_capacity_assertion(heap);
            int index = heap->tail % heap->buffer->capacity;
            heap->buffer->data[index] = entry;
            ++heap->tail;

            ocrGuid_t worker_guid = ocr_get_current_worker_guid();
            ocr_worker_t * worker = NULL;
            globalGuidProvider->getVal(globalGuidProvider, worker_guid, (u64*)&worker, NULL);
            hc_worker_t * hcWorker = (hc_worker_t *) worker;

            setLocalCost (entry, hcWorker->id);

            insertFixHeap(heap);
            if(DEBUG)verifyHeap(heap);

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
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_last ( heap_t* heap ) {
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
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_selfish ( heap_t* heap , int thiefID ) {
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
                int thiefsCheapestHeapIndex = getMostLocal(heap, thiefID);
                /* retract to last element */
                --heap->tail;
                /* make the costliest the last one by swapping it with tail */
                swapAtIndices ( heap, heap->tail % heap->buffer->capacity , (thiefsCheapestHeapIndex + heap->head) % heap->buffer->capacity );
                /* now the swap may have made the non-worst placed invalid, fix that */
                insertFixHeapAtIndex(heap, thiefsCheapestHeapIndex);
                
                /* pop tail */
                rt = (void*) heap->buffer->data[heap->tail % heap->buffer->capacity];
                if(DEBUG)verifyHeap(heap);
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
                int nLeaves = (1+size)/2;
                int currIndex, maxIndex = size - nLeaves; /*first leaf index*/;
                int maxCost = -1;
                for ( currIndex = maxIndex; currIndex < size; ++currIndex ) {
                    int currLeafCost = extractCost (heap->buffer->data[ ( currIndex + heap->head ) % heap->buffer->capacity]);
                    if ( currLeafCost > maxCost ) {
                        maxCost = currLeafCost;
                        maxIndex = currIndex;
                    }
                }

                /* retract to last element */
                --heap->tail;
                /* make the costliest the last one by swapping it with tail */
                swapAtIndices ( heap, heap->tail % heap->buffer->capacity , (maxIndex + heap->head) % heap->buffer->capacity );
                /* now the swap may have made the non-worst placed invalid, fix that */
                insertFixHeapAtIndex(heap, maxIndex);
                
                /* pop tail */
                rt = (void*) heap->buffer->data[heap->tail % heap->buffer->capacity];
                if(DEBUG)verifyHeap(heap);
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
