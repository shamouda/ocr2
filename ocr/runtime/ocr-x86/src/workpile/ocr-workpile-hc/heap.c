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

#include <stdio.h>
#include <float.h>
#include <stdlib.h>
#include <assert.h>

#include "assert.h"
#include "heap.h"
#include "hc_sysdep.h"
#include "ocr-types.h"
#include "hc_edf.h"
#include "datablock/regular/regular.h"
#include "hc.h"

#define DEBUG 1

void heap_init(heap_t * heap, void * init_value) {
    heap->lock = 0;
    heap->head = 0;
    heap->tail = 0;
    heap->nDescendantsSum = 0;
    heap->nPrioritySum = 0;
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

void locked_dequeish_heap_tail_push_no_priority (heap_t* heap, void* entry) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;
            push_capacity_assertion(heap);

            heap->buffer->data[ heap->tail++ % heap->buffer->capacity] = entry;
            heap->nDescendantsSum += ((hc_task_t*)entry)->nDescendants;
            heap->lock = 0;
        }
    }
}

#ifdef DAVINCI
#define N_SOCKETS 2
#define N_L3S_PER_SOCKET 1
#define N_L2S_PER_SOCKET 6
#endif /* DAVINCI */

#ifdef GUADALUPE
#define N_SOCKETS 2
#define N_L3S_PER_SOCKET 1
#define N_L2S_PER_SOCKET 18
#endif /* GUADALUPE */

#if defined(DAVINCI) || defined(GUADALUPE)
#define N_TOTAL_L3S ( (N_SOCKETS) * (N_L3S_PER_SOCKET))
#define N_TOTAL_L2S ( (N_SOCKETS) * (N_L2S_PER_SOCKET))

/* L2s indexed from 0 to N_TOTAL_L2S-1
 * L3s indexed from N_TOTAL_L2S to N_TOTAL_L2S + N_TOTAL_L3S - 1
 * sockets indexed from N_TOTAL_L2S + N_TOTAL_L3S to N_TOTAL_L2S + N_TOTAL_L3S + N_SOCKETS - 1 
 * */

#define MISS 1000
#define LOCAL_L3_HIT 50
#define LOCAL_L2_HIT 10

#define SOCKET_INDEX_OFFSET ((N_TOTAL_L2S)+(N_TOTAL_L3S))
#endif /*defined(DAVINCI) || defined(GUADALUPE) */

#if defined(RICE_PHI) || defined(INTEL_PHI)
#define MISS 1000
#define CORE_HIT 50
#define N_MAX_HYPERTHREADS 4
#endif /* defined(RICE_PHI) || defined(INTEL_PHI) */

static inline double costToAcquireData ( int worker_cpu_id, u64 placeTracker ) {
    double cost = 0;
#if defined(DAVINCI) || defined(GUADALUPE)
    unsigned char socket_id = worker_cpu_id/N_L2S_PER_SOCKET;
    unsigned char l2_within_socket_id = worker_cpu_id % N_L2S_PER_SOCKET;
    unsigned char l3_within_socket_id = 0;

    if (!( placeTracker & (1ULL << (N_TOTAL_L2S + N_TOTAL_L3S + socket_id)) )) { /*not in my socket*/
        cost += MISS;
    } else if (!( placeTracker & (1ULL << (N_TOTAL_L2S + socket_id*N_L3S_PER_SOCKET + l3_within_socket_id )) )) { /*in socket, not in my L3*/
        cost += MISS;
    } else if (!( placeTracker & (1ULL << (socket_id*N_L2S_PER_SOCKET + l2_within_socket_id)) )) { /*in socket, in L3, not in my L2*/
        cost += LOCAL_L3_HIT;
    } else {
        cost += LOCAL_L2_HIT;
    }
#else
#if defined(RICE_PHI) || defined(INTEL_PHI)
    unsigned char core_id = (worker_cpu_id-1)/N_MAX_HYPERTHREADS;
    if ( placeTracker & (1ULL << core_id) ) {
        cost += CORE_HIT;
    } else {
        cost += MISS;
    }
#endif /* RICE_PHI */
#endif /* DAVINCI */
    return cost;
}

static inline double costToAcquireEvent ( int worker_cpu_id, int put_worker_cpu_id ) {
    double cost = 0;
#if defined(DAVINCI) || defined(GUADALUPE)
    unsigned char worker_socket_id = worker_cpu_id/N_L2S_PER_SOCKET;
    unsigned char worker_l2_within_socket_id = worker_cpu_id % N_L2S_PER_SOCKET;

    unsigned char put_socket_id = put_worker_cpu_id/N_L2S_PER_SOCKET;
    unsigned char put_l2_within_socket_id = put_worker_cpu_id % N_L2S_PER_SOCKET;

    if ( put_socket_id != worker_socket_id ) { /*not in my socket*/
        cost += MISS;
    } else if ( put_l2_within_socket_id != worker_l2_within_socket_id ) { /*in socket, in L3, not in my L2*/
        cost += LOCAL_L3_HIT;
    } else {
        cost += LOCAL_L2_HIT;
    }
#else
#if defined(RICE_PHI) || defined(INTEL_PHI)
    unsigned char core_id = (worker_cpu_id-1)/N_MAX_HYPERTHREADS;
    unsigned char put_core_id = (put_worker_cpu_id-1)/N_MAX_HYPERTHREADS;
    if ( core_id == put_core_id ) {
        cost += CORE_HIT;
    } else {
        cost += MISS;
    }
#endif /* RICE_PHI */
#endif /* DAVINCI */
    return cost;
}

static double calculateTaskRetrievalCostFromData ( volatile void* entry, int worker_cpu_id ) {
    hc_task_t* derived = (hc_task_t*)entry;

    ocrDataBlockSimplest_t *placedDb = NULL;
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
            totalCost += costToAcquireData(worker_cpu_id, placeTracker);
        }

        curr = eventArray[++i];
    };

    return totalCost;
}

static double calculateTaskRetrievalCostFromEvents ( volatile void* entry, int worker_cpu_id ) {
    hc_task_t* derived = (hc_task_t*)entry;

    int i = 0;
    double totalCost = 0;

    ocr_event_t** eventArray = derived->awaitList->array;
    hc_event_t* curr = NULL;

    int nEvent = 0;
    for ( curr = (hc_event_t*)eventArray[0]; NULL != curr; curr = (hc_event_t*)eventArray[++nEvent] ) {
        totalCost += costToAcquireEvent (worker_cpu_id, curr->put_worker_cpu_id);
    }
    return totalCost;
}

static void setPriorityAsEventRetrievalCost ( volatile void* entry, int worker_cpu_id ) {
    hc_task_t* derived = (hc_task_t*)entry;
    derived->cost = calculateTaskRetrievalCostFromEvents ( entry, worker_cpu_id );
}

static void setPriorityAsDataRetrievalCost ( volatile void* entry, int worker_cpu_id ) {
    hc_task_t* derived = (hc_task_t*)entry;
    derived->cost = calculateTaskRetrievalCostFromData ( entry, worker_cpu_id );
}

static void setPriorityAsUserPriority ( volatile void* entry, int worker_cpu_id ) {
    hc_task_t* derived = (hc_task_t*)entry;
    derived->cost = derived->priority;
}

static inline long double extractPriority ( volatile void* entry ) {
    /* TODO implement cost extraction*/
    hc_task_t* derived = (hc_task_t*)entry;
    return derived->cost;
}

enum priority_t {EVENT_PRIORITY, DATA_PRIORITY, USER_PRIORITY};

/*sagnak awful awful hardcoding*/
/*returns cheapest task for the thief to execute by handing out its "heap" index*/
static int getMostEventLocalToThief ( heap_t* heap, int thief_cpu_id ) {
    int currHeapIndex = 0;
    int cheapestHeapIndex = 0;
    int size = heap->tail - heap->head;
    long double minCost = LDBL_MAX;
    while ( currHeapIndex < size ) {
        volatile void* currTask = heap->buffer->data[(heap->head+currHeapIndex) % heap->buffer->capacity];
        int currCost = calculateTaskRetrievalCostFromEvents (currTask, thief_cpu_id);
        if ( minCost > currCost ) {
            minCost = currCost;
            cheapestHeapIndex = currHeapIndex;
        }
        ++currHeapIndex;
    }
    return cheapestHeapIndex;
}

static int getMostDataLocalToThief ( heap_t* heap, int thief_cpu_id ) {
    int currHeapIndex = 0;
    int cheapestHeapIndex = 0;
    int size = heap->tail - heap->head;
    long double minCost = LDBL_MAX;
    while ( currHeapIndex < size ) {
        volatile void* currTask = heap->buffer->data[(heap->head+currHeapIndex) % heap->buffer->capacity];
        int currCost = calculateTaskRetrievalCostFromData (currTask, thief_cpu_id);
        if ( minCost > currCost ) {
            minCost = currCost;
            cheapestHeapIndex = currHeapIndex;
        }
        ++currHeapIndex;
    }
    return cheapestHeapIndex;
}

static inline void swapAtIndices ( heap_t* heap, int firstIndex, int secondIndex ) {
    volatile void** data = heap->buffer->data;
    volatile void* swapTemp = data[firstIndex];
    data[firstIndex] = data[secondIndex];
    data[secondIndex] = swapTemp;
}

void verifyHeapHelper ( heap_t* heap, int nElements, int firstElementIndex, int checkIndex ) {
    const int capacity = heap->buffer->capacity;

    int currHeapIndex = (checkIndex + firstElementIndex) % capacity;
    long double currPriority = extractPriority(heap->buffer->data[ currHeapIndex ]);

    if ( checkIndex*2+1 < nElements ) {
        int currLeftHeapIndex = (checkIndex*2+1 + firstElementIndex) % capacity;
        long double leftPriority = extractPriority(heap->buffer->data[currLeftHeapIndex]);
        assert ( leftPriority >= currPriority && "broken heap");
        verifyHeapHelper(heap, nElements, firstElementIndex, checkIndex*2+1);

        if ( checkIndex*2+2 < nElements ) {
            int currRightHeapIndex = (checkIndex*2+2 + firstElementIndex) % capacity;
            long double rightPriority = extractPriority(heap->buffer->data[currRightHeapIndex]);
            assert ( rightPriority >= currPriority && "broken heap");
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
    const int capacity = heap->buffer->capacity;
    volatile void** data = heap->buffer->data;

    /* set last leaf as root, as the popper already cached the extracted the root task*/
    /* one less element as root is extracted */
    data[ ( heap->head ) % capacity ] = data[ ( --heap->tail ) % capacity ];

    int firstElementIndex = heap->head % capacity;
    int nElements = heap->tail - heap->head;

    int heapFixIndex = 0;
    while ( heapFixIndex < nElements ) { //TODO 
        int currHeapIndex = (heapFixIndex + firstElementIndex) % capacity;
        long double currPriority = extractPriority(data[currHeapIndex]);

        int heapLeftChildIndex = heapFixIndex*2+1;
        if ( heapLeftChildIndex < nElements ) {
            int currLeftHeapIndex = (heapLeftChildIndex + firstElementIndex) % capacity;
            long double leftPriority = extractPriority(data[currLeftHeapIndex]);

            int heapRightChildIndex = heapFixIndex*2+2;

            if ( heapRightChildIndex < nElements ) {
                /*has left and right children */
                int currRightHeapIndex = (heapRightChildIndex + firstElementIndex) % capacity;
                long double rightPriority = extractPriority(data[currRightHeapIndex]);

                if ( leftPriority < rightPriority ) {
                    if ( leftPriority < currPriority ) {
                        swapAtIndices(heap, currLeftHeapIndex, currHeapIndex);
                        heapFixIndex = heapLeftChildIndex;
                    } else {
                        /*nothing to fix; break the loop*/
                        heapFixIndex = nElements;
                    }
                } else {
                    if ( rightPriority < currPriority ) {
                        swapAtIndices(heap, currRightHeapIndex, currHeapIndex);
                        heapFixIndex = heapRightChildIndex;
                    } else {
                        /*nothing to fix; break the loop*/
                        heapFixIndex = nElements;
                    }
                }
            } else {
                /*has only left child */
                if ( leftPriority < currPriority ) {
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
    const int capacity = heap->buffer->capacity;
    int firstElementIndex = heap->head % capacity;

    while( heapFixIndex > 0 ) {
        int currHeapIndex = (heapFixIndex + firstElementIndex) % capacity;
        long double currPriority = extractPriority(heap->buffer->data[ currHeapIndex ]);

        int parentHeapIndex = ((heapFixIndex-1)/2 + firstElementIndex) % capacity ;
        long double parentPriority = extractPriority(heap->buffer->data[parentHeapIndex]);

        if ( currPriority < parentPriority ) {
            swapAtIndices(heap, currHeapIndex, parentHeapIndex);
            /* now fix the new moved level on the tree */
            heapFixIndex = (heapFixIndex-1)/2;
        } else {
            /* nothing to fix, break the loop */
            heapFixIndex = 0;
        }
    }
}

static inline void insertFixHeap ( heap_t* heap ) {
    insertFixHeapAtIndex(heap, heap->tail - heap->head - 1);
}

static int insertSortedBetween ( heap_t* heap, int beginIndex, int endIndex, long double currPriority) {
    int indexToBeReturned = -1;

    int midRange = beginIndex + (endIndex - beginIndex)/2;
    long double midPriority = extractPriority(heap->buffer->data[midRange % heap->buffer->capacity]);
    if ( currPriority < midPriority ) {
        if ( midRange == beginIndex ) {
            indexToBeReturned = beginIndex;
        } else {
            indexToBeReturned = insertSortedBetween(heap, beginIndex, midRange - 1, currPriority);
        }
    } else if ( currPriority > midPriority ) {
        if ( midRange + 1  <=  endIndex ) {
            indexToBeReturned = insertSortedBetween(heap, midRange + 1, endIndex, currPriority);
        } else {
            indexToBeReturned = midRange+1;
        }
    } else {
        indexToBeReturned = midRange;
    }
    return indexToBeReturned;
}

static void shiftHeapRightAt ( heap_t* heap, int insertIndex ) {
    const int capacity = heap->buffer->capacity;
    volatile void** data = heap->buffer->data;
    int index = heap->tail;
    for ( ; index > insertIndex; --index ) {
        data[ (index) % capacity ] = data[ (index-1) % capacity ];
    }
    ++heap->tail;
}

static void shiftHeapLeftAt ( heap_t* heap, int deleteIndex ) {
    const int capacity = heap->buffer->capacity;
    volatile void** data = heap->buffer->data;
    int index = deleteIndex;
    const int endIndex = heap->tail-1;
    for ( ; index < endIndex; ++index ) {
        data[ (index) % capacity ] = data[ (index+1) % capacity ];
    }
    --heap->tail;
}

static inline void insertSorted ( heap_t* heap, void* entry ) {
    int insertIndex = heap->tail;
    
    if ( heap->tail != heap->head ) {
        long double currPriority = extractPriority(entry);
        insertIndex = insertSortedBetween (heap, heap->head, heap->tail - 1, currPriority);
    }

    shiftHeapRightAt (heap, insertIndex);
    heap->buffer->data[ insertIndex % heap->buffer->capacity ] = entry;
}

static int verifySortedHeap( heap_t* heap) {
    const int capacity = heap->buffer->capacity;
    volatile void** data = heap->buffer->data;
    int stillValid = 1;

    int currIndex = heap->head;
    const int endIndex = heap->tail - 1;
    for ( ; currIndex < endIndex; ++currIndex ) {
        stillValid = stillValid && (extractPriority(data[currIndex%capacity]) <= extractPriority(data[(currIndex+1)%capacity]));
        //if (!stillValid) fprintf(stderr, "[%d]-> %Le, [%d]-> %Le", currIndex, extractPriority(data[currIndex%capacity]), currIndex+1, extractPriority(data[(currIndex+1)%capacity]));
        assert(stillValid && "not sorted");
    }
    return stillValid;
}

void locked_heap_push_priority_helper (heap_t* heap, void* entry, enum priority_t p) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;

            push_capacity_assertion(heap);
            heap->buffer->data[heap->tail++ % heap->buffer->capacity] = entry;

            ocrGuid_t worker_guid = ocr_get_current_worker_guid();
            ocr_worker_t * worker = NULL;
            globalGuidProvider->getVal(globalGuidProvider, worker_guid, (u64*)&worker, NULL);

            switch(p) {
                case EVENT_PRIORITY:
                    setPriorityAsEventRetrievalCost (entry, get_worker_cpu_id(worker));
                    break;
                case DATA_PRIORITY:
                    setPriorityAsDataRetrievalCost (entry, get_worker_cpu_id(worker));
                    break;
                case USER_PRIORITY:
                    setPriorityAsUserPriority (entry, get_worker_cpu_id(worker));
                    break;
                default: assert(0 && "no such priority type for pushing"); break;
            };

            insertFixHeap(heap);
            heap->nDescendantsSum += ((hc_task_t*)entry)->nDescendants;
            heap->nPrioritySum += extractPriority(entry);
            if(DEBUG)verifyHeap(heap);

            heap->lock = 0;
        }
    }
}

void locked_heap_push_event_priority (heap_t* heap, void* entry) {
    locked_heap_push_priority_helper(heap, entry, EVENT_PRIORITY);
}

void locked_heap_push_data_priority (heap_t* heap, void* entry) {
    locked_heap_push_priority_helper(heap, entry, DATA_PRIORITY);
}

void locked_heap_push_user_priority (heap_t* heap, void* entry) {
    locked_heap_push_priority_helper(heap, entry, USER_PRIORITY);
}

void locked_heap_sorted_push_priority_helper (heap_t* heap, void* entry, enum priority_t p) {
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) { 
            success = 1;

            push_capacity_assertion(heap);

            ocrGuid_t worker_guid = ocr_get_current_worker_guid();
            ocr_worker_t * worker = NULL;
            globalGuidProvider->getVal(globalGuidProvider, worker_guid, (u64*)&worker, NULL);

            switch(p) {
                case EVENT_PRIORITY:
                    setPriorityAsEventRetrievalCost (entry, get_worker_cpu_id(worker));
                    break;
                case DATA_PRIORITY:
                    setPriorityAsDataRetrievalCost (entry, get_worker_cpu_id(worker));
                    break;
                case USER_PRIORITY:
                    setPriorityAsUserPriority (entry, get_worker_cpu_id(worker));
                    break;
                default: assert(0 && "no such priority type for sorted pushing"); break;
            };

            insertSorted(heap, entry);
            heap->nDescendantsSum += ((hc_task_t*)entry)->nDescendants;
            heap->nPrioritySum += extractPriority(entry);
            if(DEBUG) verifySortedHeap(heap);

            heap->lock = 0;
        }
    }
}

void locked_heap_sorted_push_event_priority (heap_t* heap, void* entry) {
    locked_heap_sorted_push_priority_helper(heap, entry, EVENT_PRIORITY);
}

void locked_heap_sorted_push_data_priority (heap_t* heap, void* entry) {
    locked_heap_sorted_push_priority_helper(heap, entry, DATA_PRIORITY);
}

void locked_heap_sorted_push_user_priority (heap_t* heap, void* entry) {
    locked_heap_sorted_push_priority_helper(heap, entry, USER_PRIORITY);
}

void* locked_heap_pop_priority_best ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ heap->head % heap->buffer->capacity];
                removeFixHeap(heap);
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_sorted_pop_priority_best ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ heap->head % heap->buffer->capacity];
                ++heap->head;
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifySortedHeap(heap);
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
            if ( size > 0 ) {
                --heap->tail;
                rt = (void*) heap->buffer->data[heap->tail % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_selfish_helper ( heap_t* heap , int thief_cpu_id, enum priority_t p ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                int thiefsCheapestHeapIndex = -1;
                switch (p) {
                    case EVENT_PRIORITY:
                        thiefsCheapestHeapIndex = getMostEventLocalToThief(heap, thief_cpu_id);
                        break;
                    case DATA_PRIORITY:
                        thiefsCheapestHeapIndex = getMostDataLocalToThief(heap, thief_cpu_id);
                        break;
                    default: assert(0 && "no such priority for selfish popping"); break;
                };
                /* retract to last element */
                --heap->tail;
                /* make the costliest the last one by swapping it with tail */
                swapAtIndices ( heap, heap->tail % heap->buffer->capacity , (thiefsCheapestHeapIndex + heap->head) % heap->buffer->capacity );
                /* now the swap may have made the non-worst placed invalid, fix that */
                insertFixHeapAtIndex(heap, thiefsCheapestHeapIndex);
                
                /* pop tail */
                rt = (void*) heap->buffer->data[heap->tail % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_event_priority_selfish ( heap_t* heap , int thief_cpu_id ) {
    return locked_heap_pop_priority_selfish_helper(heap, thief_cpu_id, EVENT_PRIORITY);
}

void* locked_heap_pop_data_priority_selfish ( heap_t* heap , int thief_cpu_id ) {
    return locked_heap_pop_priority_selfish_helper(heap, thief_cpu_id, DATA_PRIORITY);
}

void* locked_heap_sorted_pop_priority_selfish_helper ( heap_t* heap , int thief_cpu_id, enum priority_t p ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                int thiefsCheapestIndex = -1;
                switch (p) {
                    case EVENT_PRIORITY:
                        thiefsCheapestHeapIndex = getMostEventLocalToThief(heap, thief_cpu_id);
                        break;
                    case DATA_PRIORITY:
                        thiefsCheapestHeapIndex = getMostDataLocalToThief(heap, thief_cpu_id);
                        break;
                    default: assert(0 && "no such priority for selfish popping"); break;
                };
                rt = (void*) heap->buffer->data[heap->head + thiefsCheapestIndex % heap->buffer->capacity];

                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);

                shiftHeapLeftAt(heap, heap->head + thiefsCheapestIndex);

                if(DEBUG)verifySortedHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_sorted_pop_event_priority_selfish ( heap_t* heap , int thief_cpu_id ) {
    return locked_heap_sorted_pop_priority_selfish_helper(heap, entry, EVENT_PRIORITY);
}

void* locked_heap_sorted_pop_data_priority_selfish ( heap_t* heap , int thief_cpu_id ) {
    return locked_heap_sorted_pop_priority_selfish_helper(heap, entry, DATA_PRIORITY);
}

void* locked_heap_pop_priority_worst ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                int nLeaves = (1+size)/2;
                int currIndex, maxIndex = size - nLeaves; /*first leaf index*/;
                long double maxPriority = -1;
                for ( currIndex = maxIndex; currIndex < size; ++currIndex ) {
                    long double currLeafPriority = extractPriority (heap->buffer->data[ ( currIndex + heap->head ) % heap->buffer->capacity]);
                    if ( currLeafPriority > maxPriority ) {
                        maxPriority = currLeafPriority;
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
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifyHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_sorted_pop_priority_worst ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ --heap->tail % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                if(DEBUG)verifySortedHeap(heap);
            }
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_worst_half ( heap_t* heap, ocr_scheduler_t* thief_base) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                int tail = heap->tail;
                int nLeaves = (1+size)/2;
                int leafIndex = size - nLeaves;

                rt = (void*) heap->buffer->data[ (heap->head + leafIndex) % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                heap->nPrioritySum -= extractPriority(rt);
                int stolenIndex = 1;
                ocrGuid_t worker_guid = ocr_get_current_worker_guid();
                for ( ; stolenIndex < nLeaves; ++stolenIndex ) {
                    volatile void* stolen = heap->buffer->data[(heap->head + leafIndex + stolenIndex) % heap->buffer->capacity];
                    thief_base->give(thief_base, worker_guid, (ocrGuid_t)stolen);
                    heap->nDescendantsSum -= ((hc_task_t*)stolen)->nDescendants;
                    heap->nPrioritySum -= extractPriority(stolen);
                }
                heap->tail -= nLeaves;
            } 
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_sorted_pop_priority_worst_half ( heap_t* heap, ocr_scheduler_t* thief_base) {
    return locked_heap_pop_priority_worst_half(heap, thief_base);
}

void* locked_heap_pop_priority_worst_counting_half_helper ( heap_t* heap, ocr_scheduler_t* thief_base) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ --heap->tail % heap->buffer->capacity];

                long double prioritySumStolen = extractPriority(rt);
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;

                ocrGuid_t worker_guid = ocr_get_current_worker_guid();
                while ( prioritySumStolen < heap->nPrioritySum/2 && heap->tail - heap->head > 0 ) {
                    volatile void* stolen = heap->buffer->data[ --heap->tail % heap->buffer->capacity];
                    thief_base->give(thief_base, worker_guid, (ocrGuid_t)stolen);
                    prioritySumStolen += extractPriority(stolen);
                    heap->nDescendantsSum -= ((hc_task_t*)stolen)->nDescendants;
                }

                heap->nPrioritySum -= prioritySumStolen;
            } 
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_heap_pop_priority_worst_counting_half ( heap_t* heap, ocr_scheduler_t* thief_base) {
    return locked_heap_pop_priority_worst_counting_half_helper(heap, thief_base);
}

void* locked_heap_sorted_pop_priority_worst_counting_half ( heap_t* heap, ocr_scheduler_t* thief_base) {
    return locked_heap_pop_priority_worst_counting_half_helper(heap, thief_base);
}


void* locked_dequeish_heap_head_pop_no_priority ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ heap->head++ % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
            }
            heap->lock = 0;
        }
    }
    return rt;
}


void* locked_dequeish_heap_pop_head_half_no_priority ( heap_t* heap, ocr_scheduler_t* thief_base ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                int head = heap->head;
                int nStolen = (1+size)/2;
                heap->head = nStolen + head;

                rt = (void*) heap->buffer->data[head % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
                int stolenIndex = 1;
                ocrGuid_t worker_guid = ocr_get_current_worker_guid();
                for ( ; stolenIndex < nStolen; ++stolenIndex ) {
                    volatile void* stolen = heap->buffer->data[(stolenIndex + head) % heap->buffer->capacity];
                    thief_base->give(thief_base, worker_guid, (ocrGuid_t)stolen);
                    heap->nDescendantsSum -= ((hc_task_t*)stolen)->nDescendants;
                }
            } 
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_dequeish_heap_pop_head_counting_half_no_priority ( heap_t* heap, ocr_scheduler_t* thief_base ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[heap->head % heap->buffer->capacity];
                ++heap->head;

                long double descendantsSumStolen = ((hc_task_t*) rt) -> nDescendants;

                ocrGuid_t worker_guid = ocr_get_current_worker_guid();
                while ( descendantsSumStolen < heap->nDescendantsSum/2 && heap->tail - heap->head > 0 ) {
                    volatile void* stolen = heap->buffer->data[ heap->head % heap->buffer->capacity];
                    ++heap->head;
                    thief_base->give(thief_base, worker_guid, (ocrGuid_t)stolen);
                    descendantsSumStolen += ((hc_task_t*)stolen) -> nDescendants;
                }

                heap->nDescendantsSum -= descendantsSumStolen;
            } 
            heap->lock = 0;
        }
    }
    return rt;
}

void* locked_dequeish_heap_tail_pop_no_priority ( heap_t* heap ) {
    void * rt = NULL;
    int success = 0;
    while (!success) {
        if ( hc_cas(&heap->lock, 0, 1) ) {
            success = 1;

            int size = heap->tail - heap->head;
            if ( size > 0 ) {
                rt = (void*) heap->buffer->data[ --heap->tail % heap->buffer->capacity];
                heap->nDescendantsSum -= ((hc_task_t*)rt)->nDescendants;
            }
            heap->lock = 0;
        }
    }
    return rt;
}
