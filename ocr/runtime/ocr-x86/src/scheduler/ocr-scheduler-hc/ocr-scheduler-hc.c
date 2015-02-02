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

#include "datablock/regular/regular.h"
#include "ocr-runtime.h"
#include "hc.h"

/******************************************************/
/* OCR-HC SCHEDULER                                   */
/******************************************************/

void hc_scheduler_create(ocr_scheduler_t * base, void * per_type_configuration, void * per_instance_configuration) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    scheduler_configuration *mapper = (scheduler_configuration*)per_instance_configuration;

    derived->worker_id_begin = mapper->worker_id_begin;
    derived->worker_id_end = mapper->worker_id_end;
    derived->n_workers_per_scheduler = 1 + derived->worker_id_end - derived->worker_id_begin;
}

void hc_scheduler_destruct(ocr_scheduler_t * scheduler) {
    // just free self, workpiles are not allocated by the scheduler
    free(scheduler);
}
/* sagnak
 * for pool mapping worker id 0..N is used rather than cpu_id's from the bind file*/
static inline ocr_workpile_t * hc_scheduler_pop_mapping_one_to_one (ocr_scheduler_t* base, ocr_worker_t* w ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return derived->pools[get_worker_id(w) % derived->n_workers_per_scheduler ];
}

static ocr_workpile_t* pop_mapping_deprecated_assert (ocr_scheduler_t* base, ocr_worker_t* w ) {
    assert (0 && "do not use pop mapping anymore");
    return NULL;
}

static inline ocr_workpile_t * hc_scheduler_push_mapping_one_to_one (ocr_scheduler_t* base, ocr_worker_t* w ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return derived->pools[get_worker_id(w) % derived->n_workers_per_scheduler];
}

#ifdef DAVINCI
#define N_SOCKETS 2
#define N_L3S_PER_SOCKET 1
#define N_L2S_PER_SOCKET 6
#define N_L1S_PER_SOCKET 6
#define N_TOTAL_L3S ( (N_SOCKETS) * (N_L3S_PER_SOCKET))
#define N_TOTAL_L2S ( (N_SOCKETS) * (N_L2S_PER_SOCKET))
#define N_TOTAL_L1S ( (N_SOCKETS) * (N_L1S_PER_SOCKET))

/* L1s indexed from 0 to N_TOTAL_L1S-1
 * L2s indexed from N_TOTAL_L1S to N_TOTAL_L1S + N_TOTAL_L2S - 1
 * L3s indexed from N_TOTAL_L1S + N_TOTAL_L2S to N_TOTAL_L1S + N_TOTAL_L2S + N_TOTAL_L3S - 1
 * sockets indexed from N_TOTAL_L1S + N_TOTAL_L2S + N_TOTAL_L3S to N_TOTAL_L1S + N_TOTAL_L2S + N_TOTAL_L3S + N_SOCKETS - 1 
 * */

#define SOCKET_INDEX_OFFSET ((N_TOTAL_L1S)+(N_TOTAL_L2S)+(N_TOTAL_L3S))

#endif /* DAVINCI */

#ifdef RICE_PHI
#define N_MAX_HYPERTHREADS 4
#define N_PHI_CORES 55
#endif /*RICE_PHI*/


#ifdef DAVINCI
/* sagnak quite hacky and hardcode-y */
static int davinciMostDataLocalWorker ( ocrGuid_t taskGuid ) {
    hc_task_t* derivedTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, taskGuid, (u64*)&derivedTask, NULL);

    ocrDataBlockSimplest_t *placedDb = NULL;
    ocrGuid_t dbGuid = UNINITIALIZED_GUID;

    int nEvents = 0;

    ocr_event_t** eventArray = derivedTask->awaitList->array;
    ocr_event_t* curr = eventArray[0];

    char socketPresence[ N_SOCKETS ] = {0,0};
    while ( NULL != curr ) {
        dbGuid = curr->get(curr);

        if(dbGuid != NULL_GUID) {
            globalGuidProvider->getVal(globalGuidProvider, dbGuid, (u64*)&placedDb, NULL);
            u64 placeTracker = placedDb->placeTracker.existInPlaces;
            int socketIndex = 0;
            for ( ; socketIndex < N_SOCKETS ; ++socketIndex ) {
                if (placeTracker & (1ULL << (SOCKET_INDEX_OFFSET + socketIndex))) { //in socket 'socketIndex'
                    ++socketPresence[socketIndex];
                }
            }
        }
        curr = eventArray[++nEvents];
    };

    int mostLocalSocketIndex = socketPresence[1] > socketPresence[0];

    nEvents = 0;
    curr = eventArray[0];

    char l2Presence[ N_L2S_PER_SOCKET ] = {0,0,0,0,0,0};
    int l2IndexOffset = N_TOTAL_L1S + mostLocalSocketIndex * N_L2S_PER_SOCKET;

    while ( NULL != curr ) {
        dbGuid = curr->get(curr);

        if(dbGuid != NULL_GUID) {
            globalGuidProvider->getVal(globalGuidProvider, dbGuid, (u64*)&placedDb, NULL);
            u64 placeTracker = placedDb->placeTracker.existInPlaces;
            int l2index = 0;
            for ( ; l2index < N_L2S_PER_SOCKET ; ++l2index ) {
                if (placeTracker & (1ULL << ( l2IndexOffset + l2index ))) { 
                    ++l2Presence[l2index];
                }
            }
        }

        curr = eventArray[++nEvents];
    };

    int l2index = 0;
    int maxIndex = 0;
    for ( ; l2index < N_L2S_PER_SOCKET ; ++l2index ) {
        if ( l2Presence[maxIndex] < l2Presence[l2index] ) {
            maxIndex = l2index;
        }
    }
    return mostLocalSocketIndex*N_L2S_PER_SOCKET + maxIndex;
}

static int davinciMostEventLocalWorker ( ocrGuid_t taskGuid ) {
    hc_task_t* derivedTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, taskGuid, (u64*)&derivedTask, NULL);

    ocr_event_t** eventArray = derivedTask->awaitList->array;
    hc_event_t* curr = NULL;

    char socketPresence[ N_SOCKETS ] = {0,0};
    int nEvent = 0;
    for ( curr = (hc_event_t*)eventArray[0]; NULL != curr; curr = (hc_event_t*)eventArray[++nEvent] ) {
        ++socketPresence [ curr->put_worker_cpu_id % N_L2S_PER_SOCKET ];
    }

    int mostLocalSocketIndex = socketPresence[1] > socketPresence[0];
    int mostLocalSocketIndexOffset = mostLocalSocketIndex * N_L2S_PER_SOCKET;

    char l2Presence[ N_L2S_PER_SOCKET ] = {0,0,0,0,0,0};

    nEvent = 0;
    for ( curr = (hc_event_t*)eventArray[0]; NULL != curr; curr = (hc_event_t*)eventArray[++nEvent] ) {
        int put_worker_cpu_id = curr->put_worker_cpu_id;
        if ( mostLocalSocketIndex == put_worker_cpu_id % N_L2S_PER_SOCKET ) {
            ++l2Presence [ put_worker_cpu_id - mostLocalSocketIndexOffset ];
        }
    }

    int l2index = 0;
    int maxIndex = 0;
    for ( ; l2index < N_L2S_PER_SOCKET ; ++l2index ) {
        if ( l2Presence[maxIndex] < l2Presence[l2index] ) {
            maxIndex = l2index;
        }
    }
    return mostLocalSocketIndexOffset + maxIndex;
}
#endif /* DAVINCI */

#ifdef RICE_PHI
/* sagnak quite hacky and hardcode-y */
static int ricePhiMostDataLocalWorker ( ocrGuid_t taskGuid ) {
    hc_task_t* derivedTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, taskGuid, (u64*)&derivedTask, NULL);

    ocrDataBlockSimplest_t *placedDb = NULL;
    ocrGuid_t dbGuid = UNINITIALIZED_GUID;

    int nEvents = 0;

    ocr_event_t** eventArray = derivedTask->awaitList->array;
    ocr_event_t* curr = eventArray[0];

    char corePresence[ N_PHI_CORES ];
    int i = 0;
    for (; i < N_PHI_CORES; ++i ) {
        corePresence[i] = 0;
    }

    while ( NULL != curr ) {
        dbGuid = curr->get(curr);

        if(dbGuid != NULL_GUID) {
            globalGuidProvider->getVal(globalGuidProvider, dbGuid, (u64*)&placedDb, NULL);
            u64 placeTracker = placedDb->placeTracker.existInPlaces;
            int socketIndex = 0;
            while ( 0 != placeTracker ) {
                if (placeTracker & 0x1) ++corePresence[socketIndex];
                ++socketIndex;
                placeTracker >>= 1;
            }
        }
        curr = eventArray[++nEvents];
    };

    int maxIndex = 0;
    for ( i = 0; i < N_PHI_CORES; ++i ) {
        if ( corePresence[maxIndex] < corePresence[i] ) {
            maxIndex = i;
        }
    }
    /* sagnak TODO */
    /*cpu_id is 1 + maxIndex * N_HYPERTHREADS and I will assume worker id is maxIndex * N_HYPERTHREADS which will only work for nHyperThread 4*/
    return maxIndex * N_MAX_HYPERTHREADS;
}

static int ricePhiMostEventLocalWorker ( ocrGuid_t taskGuid ) {
    hc_task_t* derivedTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, taskGuid, (u64*)&derivedTask, NULL);

    ocr_event_t** eventArray = derivedTask->awaitList->array;
    hc_event_t* curr = NULL;

    char corePresence[ N_PHI_CORES ];
    int i = 0;
    for (; i < N_PHI_CORES; ++i ) {
        corePresence[i] = 0;
    }
    int nEvent = 0;
    for ( curr = (hc_event_t*)eventArray[0]; NULL != curr; curr = (hc_event_t*)eventArray[++nEvent] ) {
        ++corePresence [ (curr->put_worker_cpu_id-1)/N_MAX_HYPERTHREADS];
    }

    int maxIndex = 0;
    for ( i = 0; i < N_PHI_CORES; ++i ) {
        if ( corePresence[maxIndex] < corePresence[i] ) {
            maxIndex = i;
        }
    }
    return maxIndex * N_MAX_HYPERTHREADS;
}
#endif /* RICE_PHI */

static inline int calculateMostDataLocalWorker ( ocrGuid_t taskGuid ) {
#ifdef DAVINCI
    return davinciMostDataLocalWorker (taskGuid); 
#else
#ifdef RICE_PHI
    return ricePhiMostDataLocalWorker (taskGuid);
#else
    assert(0 && "Can not calculate most local worker for architecture");
    return -1;
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

/* sagnak quite hacky and hardcode-y */
static int calculateMostEventLocalWorker ( ocrGuid_t taskGuid ) {
#ifdef DAVINCI
    return davinciMostEventLocalWorker ( taskGuid );
#else
#ifdef RICE_PHI
    return ricePhiMostEventLocalWorker ( taskGuid );
#else
    assert(0 && "Can not calculate most event worker for architecture");
    return -1;
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

static inline ocr_workpile_t * hc_scheduler_push_mapping_most_data_local (ocr_scheduler_t* base, ocr_worker_t* w, ocrGuid_t taskGuid ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return derived->pools[calculateMostDataLocalWorker(taskGuid)];
}

static inline ocr_workpile_t * hc_scheduler_push_mapping_most_event_local (ocr_scheduler_t* base, ocr_worker_t* w, ocrGuid_t taskGuid ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return derived->pools[calculateMostEventLocalWorker(taskGuid)];
}

static ocr_workpile_t * push_mapping_deprecated_assert (ocr_scheduler_t* base, ocr_worker_t* w ) {
    assert ( 0 && "do not use push mapping anymore");
    return NULL;
}

static inline workpile_iterator_t* hc_scheduler_cyclical_steal_mapping_one_to_all_but_self (ocr_scheduler_t* base, ocr_worker_t* w ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return workpile_iterator_constructor(get_worker_id(w)% derived->n_workers_per_scheduler, derived->n_pools, derived->pools);
}

static inline ocr_workpile_t* hc_scheduler_random_steal_mapping (ocr_scheduler_t* base, ocr_worker_t* w ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    int scope = derived->n_workers_per_scheduler;
    int my_id = get_worker_id(w) % scope;
    int victim_id = my_id ;
    do {
        victim_id = rand() %scope;
    } while (victim_id == my_id);

    return derived->pools[victim_id];
}

static workpile_iterator_t* steal_mapping_deprecated_assert (ocr_scheduler_t* base, ocr_worker_t* w ) {
    assert ( 0 && "do not use steal mapping anymore");
    return NULL;
}

/*TODO sagnak hardcoded BAD */
static ocrGuid_t hc_scheduler_local_pop_then_hier_cyclic_steal (ocr_scheduler_t* base, ocrGuid_t workerGuid ) {
#ifdef DAVINCI
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int worker_cpu_id = get_worker_cpu_id(w);
        const int workerID = get_worker_id(w);
        const int socketID = worker_cpu_id / N_L1S_PER_SOCKET;
        int iteration = 1;

        for ( ; (iteration < N_L1S_PER_SOCKET) && (NULL_GUID == popped); ++iteration ) {
            ocr_workpile_t * victim = derived->pools[ (workerID + iteration) % N_L1S_PER_SOCKET + socketID * N_L1S_PER_SOCKET ];
            popped = victim->steal(victim, worker_cpu_id,base);
        }
        if ( NULL_GUID == popped ) {
            const int otherSocketID = 1 - socketID;

            for ( iteration = 0; (iteration < N_L1S_PER_SOCKET ) && (NULL_GUID == popped); ++iteration ) {
                ocr_workpile_t * victim = derived->pools[ (workerID + iteration) % N_L1S_PER_SOCKET + otherSocketID * N_L1S_PER_SOCKET ];
                popped = victim->steal(victim, worker_cpu_id,base);
            }

        }
    }
    return popped;
#else
#ifdef RICE_PHI
    assert(0 && "Xeon Phi and hierarchical scheduling seems meh");
#else
    assert(0 && "can not calculate hierarchical cyclical stealing for architecture");
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

/*TODO sagnak hardcoded BAD */
static ocrGuid_t hc_scheduler_local_pop_then_hier_random_steal (ocr_scheduler_t* base, ocrGuid_t workerGuid) {
#ifdef DAVINCI
#define N_TURNS 2
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int worker_cpu_id = get_worker_cpu_id(w);
        const int socketID = worker_cpu_id / N_L1S_PER_SOCKET;

        const int nTrials = N_TURNS * N_L1S_PER_SOCKET;
        int trial = 0;

        for ( ; ( trial < nTrials ) && (NULL_GUID == popped); ++trial ) {
            int currVictimID = socketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
            if ( worker_cpu_id != currVictimID ) {
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, worker_cpu_id,base);
            }
        }
        if ( NULL_GUID == popped ) {
            const int otherSocketID = 1 - socketID;
            int trial = 0;
            for ( ; ( trial < nTrials ) && (NULL_GUID == popped) ; ++trial ) {
                int currVictimID = otherSocketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, worker_cpu_id,base);
            }
        }
    }
    return popped;
#else
#ifdef RICE_PHI
    assert(0 && "Xeon Phi and hierarchical scheduling seems meh");
#else
    assert(0 && "can not calculate hierarchical random stealing for architecture");
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

static ocrGuid_t hc_scheduler_local_pop_then_socket_random_steal (ocr_scheduler_t* base, ocrGuid_t workerGuid ) {
#ifdef DAVINCI
#define N_TURNS 2
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int worker_cpu_id = get_worker_cpu_id(w);
        const int socketID = worker_cpu_id / N_L1S_PER_SOCKET;

        const int nTrials = N_TURNS * N_L1S_PER_SOCKET;
        int trial = 0;

        for ( ; ( trial < nTrials ) && (NULL_GUID == popped); ++trial ) {
            int currVictimID = socketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
            if ( worker_cpu_id != currVictimID ) {
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, worker_cpu_id,base);
            }
        }
    }
    return popped;
#else
#ifdef RICE_PHI
    assert(0 && "Xeon Phi and hierarchical scheduling seems meh");
#else
    assert(0 && "can not calculate socket stealing for architecture");
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

static ocrGuid_t hc_scheduler_local_pop_then_cyclic_steal (ocr_scheduler_t* base, ocrGuid_t workerGuid) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        workpile_iterator_t* it = hc_scheduler_cyclical_steal_mapping_one_to_all_but_self(base, w);
        while ( it->hasNext(it) && (NULL_GUID == popped)) {
            ocr_workpile_t * next = it->next(it);
            popped = next->steal(next, get_worker_cpu_id(w),base);
        }
        workpile_iterator_destructor(it);
    }
    return popped;
}

static ocrGuid_t hc_scheduler_local_pop_then_random_steal (ocr_scheduler_t* base, ocrGuid_t workerGuid) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);

    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    int scope = 2 * derived->n_workers_per_scheduler;
    int tries = 0;

    while ( NULL_GUID == popped && ++tries < scope) {
        ocr_workpile_t * victim = hc_scheduler_random_steal_mapping(base,w);
        popped = victim->steal(victim, get_worker_cpu_id(w),base);
    }
    return popped;
}

static void hc_scheduler_local_push (ocr_scheduler_t* base, ocrGuid_t workerGuid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_one_to_one(base, w);
    wp_to_push->push(wp_to_push,tid);
}

static void hc_scheduler_data_locality_push (ocr_scheduler_t* base, ocrGuid_t workerGuid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_most_data_local (base, w, tid);
    wp_to_push->push(wp_to_push,tid);
}

static void hc_scheduler_event_locality_push (ocr_scheduler_t* base, ocrGuid_t workerGuid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_most_event_local (base, w, tid);
    wp_to_push->push(wp_to_push,tid);
}

static void hc_scheduler_usersocket_push (ocr_scheduler_t* base, ocrGuid_t workerGuid, ocrGuid_t tid ) {
#ifdef DAVINCI
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, workerGuid, (u64*)&w, NULL);

    ocr_task_t* baseTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, tid, (u64*)&baseTask, NULL);
    long long int socketID = (long long int) ((void*)baseTask->params);

    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    int scope = derived->n_workers_per_scheduler;
    int my_id = get_worker_id(w) % scope;
    int idWithinSocket = my_id % N_L1S_PER_SOCKET;
    
    ocr_workpile_t * wp_to_push = derived->pools[ idWithinSocket + socketID * N_L1S_PER_SOCKET ];
    wp_to_push->push(wp_to_push,tid);
#else
#ifdef RICE_PHI
    assert(0 && "Xeon Phi and hierarchical scheduling seems meh");
#else
    assert(0 && "can not calculate user socket pushing for architecture");
#endif /* RICE_PHI */
#endif /* DAVINCI */
}

/**!
 * Mapping function many-to-one to map a set of workpiles to a scheduler instance
 */
void hc_ocr_module_map_workpiles_to_schedulers(void * self_module, ocr_module_kind kind,
                                               size_t nb_instances, void ** ptr_instances) {
    // Checking mapping conforms to what we're expecting in this implementation
    assert(kind == OCR_WORKPILE);
    hc_scheduler_t * scheduler = (hc_scheduler_t *) self_module;
    scheduler->n_pools = nb_instances;
    scheduler->pools = (ocr_workpile_t **)ptr_instances;
}

static inline void hc_scheduler_constructor_common_initializer ( ocr_scheduler_t* base ) {
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = hc_ocr_module_map_workpiles_to_schedulers;
    base -> create = hc_scheduler_create;
    base -> destruct = hc_scheduler_destruct;
    base -> pop_mapping = pop_mapping_deprecated_assert;
    base -> push_mapping = push_mapping_deprecated_assert;
    base -> steal_mapping = steal_mapping_deprecated_assert;
}

ocr_scheduler_t* hc_randomvictim_localpush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_random_steal;
    base -> give = hc_scheduler_local_push;
    return base;
}

ocr_scheduler_t* hc_randomvictim_datalocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_random_steal;
    base -> give = hc_scheduler_data_locality_push;
    return base;
}

ocr_scheduler_t* hc_randomvictim_eventlocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_random_steal;
    base -> give = hc_scheduler_event_locality_push;
    return base;
}

ocr_scheduler_t* hc_cyclicvictim_localpush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_cyclic_steal;
    base -> give = hc_scheduler_local_push;
    return base;
}

ocr_scheduler_t* hc_cyclicvictim_datalocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_cyclic_steal;
    base -> give = hc_scheduler_data_locality_push;
    return base;
}

ocr_scheduler_t* hc_cyclicvictim_eventlocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_cyclic_steal;
    base -> give = hc_scheduler_event_locality_push;
    return base;
}

ocr_scheduler_t* hc_hiercyclicvictim_localpush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_cyclic_steal;
    base -> give = hc_scheduler_local_push;
    return base;
}

ocr_scheduler_t* hc_hiercyclicvictim_datalocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_cyclic_steal;
    base -> give = hc_scheduler_data_locality_push;
    return base;
}

ocr_scheduler_t* hc_hiercyclicvictim_eventlocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_cyclic_steal;
    base -> give = hc_scheduler_event_locality_push;
    return base;
}

ocr_scheduler_t* hc_hierrandomvictim_localpush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_random_steal;
    base -> give = hc_scheduler_local_push;
    return base;
}

ocr_scheduler_t* hc_hierrandomvictim_datalocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_random_steal;
    base -> give = hc_scheduler_data_locality_push;
    return base;
}

ocr_scheduler_t* hc_hierrandomvictim_eventlocalitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_random_steal;
    base -> give = hc_scheduler_event_locality_push;
    return base;
}

ocr_scheduler_t* hc_socketonlyvictim_usersocketpush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_socket_random_steal;
    base -> give = hc_scheduler_usersocket_push;
    return base;
}

ocr_scheduler_t* hc_scheduler_constructor() {
    return hc_randomvictim_localpush_scheduler_constructor();
}
