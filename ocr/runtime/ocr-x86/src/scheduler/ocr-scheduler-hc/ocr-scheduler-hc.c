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

#define DAVINCI

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

#else

#endif

/* sagnak quite hacky and hardcode-y */
static int calculateMostLocalWorker ( ocrGuid_t taskGuid ) {
#ifdef DAVINCI
    hc_task_t* derivedTask = NULL;
    globalGuidProvider->getVal(globalGuidProvider, taskGuid, (u64*)&derivedTask, NULL);

    ocrDataBlockPlaced_t *placedDb = NULL;
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
#else
    assert(0 && "");
#endif
}

static inline ocr_workpile_t * hc_scheduler_push_mapping_most_local (ocr_scheduler_t* base, ocr_worker_t* w, ocrGuid_t taskGuid ) {
    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    return derived->pools[calculateMostLocalWorker(taskGuid)];
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
static ocrGuid_t hc_scheduler_local_pop_then_hier_cyclic_steal (ocr_scheduler_t* base, ocrGuid_t wid ) {
#ifdef DAVINCI
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int workerID = get_worker_id(w);
        const int socketID = workerID / N_L1S_PER_SOCKET;
        int iteration = 1;

        for ( ; (iteration < N_L1S_PER_SOCKET) && (NULL_GUID == popped); ++iteration ) {
            ocr_workpile_t * victim = derived->pools[ (workerID + iteration) % N_L1S_PER_SOCKET + socketID * N_L1S_PER_SOCKET ];
            popped = victim->steal(victim, workerID);
        }
        if ( NULL_GUID == popped ) {
            const int otherSocketID = 1 - socketID;

            for ( iteration = 0; (iteration < N_L1S_PER_SOCKET ) && (NULL_GUID == popped); ++iteration ) {
                ocr_workpile_t * victim = derived->pools[ (workerID + iteration) % N_L1S_PER_SOCKET + otherSocketID * N_L1S_PER_SOCKET ];
                popped = victim->steal(victim, workerID);
            }

        }
    }
    return popped;
#else
    assert(0 && "");
#endif
}

/*TODO sagnak hardcoded BAD */
static ocrGuid_t hc_scheduler_local_pop_then_hier_random_steal (ocr_scheduler_t* base, ocrGuid_t wid ) {
#ifdef DAVINCI
#define N_TURNS 2
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int workerID = get_worker_id(w);
        const int socketID = workerID / N_L1S_PER_SOCKET;

        const int nTrials = N_TURNS * N_L1S_PER_SOCKET;
        int trial = 0;

        for ( ; ( trial < nTrials ) && (NULL_GUID == popped); ++trial ) {
            int currVictimID = socketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
            if ( workerID != currVictimID ) {
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, workerID);
            }
        }
        if ( NULL_GUID == popped ) {
            const int otherSocketID = 1 - socketID;
            int trial = 0;
            for ( ; ( trial < nTrials ) && (NULL_GUID == popped) ; ++trial ) {
                int currVictimID = otherSocketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, workerID);
            }
        }
    }
    return popped;
#else
    assert(0 && "");
#endif
}

static ocrGuid_t hc_scheduler_local_pop_then_socket_random_steal (ocr_scheduler_t* base, ocrGuid_t wid ) {
#ifdef DAVINCI
#define N_TURNS 2
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        hc_scheduler_t* derived = (hc_scheduler_t*) base;

        const int workerID = get_worker_id(w);
        const int socketID = workerID / N_L1S_PER_SOCKET;

        const int nTrials = N_TURNS * N_L1S_PER_SOCKET;
        int trial = 0;

        for ( ; ( trial < nTrials ) && (NULL_GUID == popped); ++trial ) {
            int currVictimID = socketID*N_L1S_PER_SOCKET + (rand() % N_L1S_PER_SOCKET );
            if ( workerID != currVictimID ) {
                ocr_workpile_t * victim = derived->pools[currVictimID];
                popped = victim->steal(victim, workerID);
            }
        }
    }
    return popped;
#else
    assert(0 && "");
#endif
}

static ocrGuid_t hc_scheduler_local_pop_then_cyclic_steal (ocr_scheduler_t* base, ocrGuid_t wid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);
    if ( NULL_GUID == popped ) {
        workpile_iterator_t* it = hc_scheduler_cyclical_steal_mapping_one_to_all_but_self(base, w);
        while ( it->hasNext(it) && (NULL_GUID == popped)) {
            ocr_workpile_t * next = it->next(it);
            popped = next->steal(next, get_worker_id(w));
        }
        workpile_iterator_destructor(it);
    }
    return popped;
}

static ocrGuid_t hc_scheduler_local_pop_then_random_steal (ocr_scheduler_t* base, ocrGuid_t wid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
    ocrGuid_t popped = wp_to_pop->pop(wp_to_pop);

    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    int scope = 2 * derived->n_workers_per_scheduler;
    int tries = 0;

    while ( NULL_GUID == popped && ++tries < scope) {
        ocr_workpile_t * victim = hc_scheduler_random_steal_mapping(base,w);
        popped = victim->steal(victim, get_worker_id(w));
    }
    return popped;
}

static void hc_scheduler_local_push (ocr_scheduler_t* base, ocrGuid_t wid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_one_to_one(base, w);
    wp_to_push->push(wp_to_push,tid);
}

static void hc_scheduler_locality_push (ocr_scheduler_t* base, ocrGuid_t wid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_most_local (base, w, tid);
    wp_to_push->push(wp_to_push,tid);
}

static void hc_scheduler_usersocket_push (ocr_scheduler_t* base, ocrGuid_t wid, ocrGuid_t tid ) {
#ifdef DAVINCI
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

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
    assert ( 0 && "" );
#endif
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

ocr_scheduler_t* hc_randomvictim_localitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_random_steal;
    base -> give = hc_scheduler_locality_push;
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

ocr_scheduler_t* hc_cyclicvictim_localitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_cyclic_steal;
    base -> give = hc_scheduler_locality_push;
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

ocr_scheduler_t* hc_hiercyclicvictim_localitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_cyclic_steal;
    base -> give = hc_scheduler_locality_push;
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

ocr_scheduler_t* hc_hierrandomvictim_localitypush_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    hc_scheduler_constructor_common_initializer(base);
    base -> take = hc_scheduler_local_pop_then_hier_random_steal;
    base -> give = hc_scheduler_locality_push;
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

ocrGuid_t hc_placed_scheduler_take (ocr_scheduler_t* base, ocrGuid_t wid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    ocrGuid_t popped = NULL_GUID;

    int worker_id = get_worker_id(w);

    hc_scheduler_t* derived = (hc_scheduler_t*) base;
    if ( worker_id >= derived->worker_id_begin && worker_id <= derived->worker_id_end ) {
        ocr_workpile_t * wp_to_pop = hc_scheduler_pop_mapping_one_to_one(base, w);
        popped = wp_to_pop->pop(wp_to_pop);
        /*TODO sagnak I hard-coded a no intra-scheduler stealing here; BAD */
        if ( NULL_GUID == popped ) {
            // TODO sagnak steal from places
            ocr_policy_domain_t* policyDomain = base->domain;
            popped = policyDomain->handIn(policyDomain, policyDomain, wid);
        }
    } else {
        // TODO sagnak oooh BAD BAD hardcoding yet again
        ocr_workpile_t* victim = derived->pools[0];
        popped = victim->steal(victim, worker_id);
    }

    return popped;
}

void hc_placed_scheduler_give (ocr_scheduler_t* base, ocrGuid_t wid, ocrGuid_t tid ) {
    ocr_worker_t* w = NULL;
    globalGuidProvider->getVal(globalGuidProvider, wid, (u64*)&w, NULL);

    // TODO sagnak calculate which 'place' to push
    ocr_workpile_t * wp_to_push = hc_scheduler_push_mapping_one_to_one(base, w);
    wp_to_push->push(wp_to_push,tid);
}

ocr_scheduler_t* hc_placed_scheduler_constructor() {
    hc_scheduler_t* derived = (hc_scheduler_t*) malloc(sizeof(hc_scheduler_t));
    ocr_scheduler_t* base = (ocr_scheduler_t*)derived;
    ocr_module_t * module_base = (ocr_module_t *) base;
    module_base->map_fct = hc_ocr_module_map_workpiles_to_schedulers;
    base -> create = hc_scheduler_create;
    base -> destruct = hc_scheduler_destruct;
    base -> pop_mapping = pop_mapping_deprecated_assert;
    base -> push_mapping = push_mapping_deprecated_assert;
    base -> steal_mapping = steal_mapping_deprecated_assert;
    base -> take = hc_placed_scheduler_take;
    base -> give = hc_placed_scheduler_give;
    return base;
}
