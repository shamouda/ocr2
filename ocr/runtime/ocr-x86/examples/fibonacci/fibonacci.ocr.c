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

#include "ocr.h"
#include <assert.h>
#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>
#ifdef HPCTOOLKIT
#include <hpctoolkit.h>
#endif
#ifdef PAPI 
#include "instrument.h"
#endif

#define FLAGS 0xdead
#define PROPERTIES 0xdead

struct timeval a,b,c,d;

typedef unsigned long long int u64_t;

u64_t efficientSerialFib(int n) {
    u64_t prev = 0;
    u64_t curr = 1;

    if ( n > 0 ) {
        while ( n-- > 1 ) {
            u64_t next = curr+prev;
            prev = curr;
            curr = next;
        }
    } else {
        curr = 0;
    }
    return curr;
}

u64_t inefficientSerialFib(int n) {
    switch(n) {
        case 0: return 0; break;
        case 1: return 1; break;
        default: return inefficientSerialFib(n-2) + inefficientSerialFib(n-1);
                break;
    };
    return 0ULL;
}

static inline int nLeaves( int depth ) {
    return efficientSerialFib(depth);
}

static inline int nNodes ( int depth ) {
    return 2*efficientSerialFib(depth+2) - 1;
}

static inline int computeNDescendants( int depth, int largestN ){
#ifdef ACCURATE_OPTIMISTIC
    return 3*efficientSerialFib(depth+2)-3 + largestN-depth;
#endif
#ifdef ACCURATE_PESSIMISTIC
    return 3*efficientSerialFib(depth+2)-3;
#endif
#ifdef LEAF 
    return efficientSerialFib(depth+2);
#endif
#ifdef DEFAULT
    return 0;
#endif
}

u8 fibonacci_join_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    u64_t *func_args = (u64_t*)paramv;
    int n = (int) func_args[0];
    int cutoff = (int) func_args[1];
    ocrGuid_t* out_db_guids = (ocrGuid_t*) func_args[2];
    ocrGuid_t* out_event_guids = (ocrGuid_t*) func_args[3];

    u64_t*  leftChild = (u64_t*) (depv[0].ptr);
    u64_t* rightChild = (u64_t*) (depv[1].ptr);

    void* temp_db;
    ocrDbCreate( &(out_db_guids[0]), &temp_db, sizeof(u64_t), FLAGS, NULL, NO_ALLOC );
    *((u64_t*)temp_db)=*leftChild+*rightChild;

    ocrEventSatisfy(out_event_guids[0], out_db_guids[0]);

    free(paramv);
}

inline static void join_task_prescriber ( int largestN, int n, int cutoff, ocrGuid_t* out_db_guids, ocrGuid_t* out_event_guids ) { 
    u64_t *func_args = (u64_t *)malloc(5*sizeof(u64_t));
    func_args[0] = n;
    func_args[1] = cutoff;
    func_args[2] = (u64_t)out_db_guids;
    func_args[3] = (u64_t)out_event_guids;
    func_args[4] = largestN;

    long double nDescendants = computeNDescendants(n-cutoff,largestN);
    long double priority = nDescendants;
    ocrGuid_t task_guid;
    ocrEdtCreate(&task_guid, fibonacci_join_task, 5, NULL, (void**)func_args, PROPERTIES, 2, NULL, nDescendants, priority);

    ocrAddDependence(out_event_guids[1], task_guid, 0);
    ocrAddDependence(out_event_guids[nNodes(n-cutoff-1)+1], task_guid, 1);
    ocrEdtSchedule(task_guid);
}

void fork_task_prescriber ( int, int, int, ocrGuid_t*, ocrGuid_t*);

u8 fibonacci_fork_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    u64_t *func_args = (u64_t*)paramv;
    int n = (int) func_args[0];
    int cutoff = (int) func_args[1];
    ocrGuid_t* out_db_guids = (ocrGuid_t*) func_args[2];
    ocrGuid_t* out_event_guids = (ocrGuid_t*) func_args[3];
    int largestN  = (int) func_args[4];

    if ( n > cutoff ) {
        fork_task_prescriber(largestN, n-1, cutoff, (ocrGuid_t*)(&(out_db_guids[1])), (ocrGuid_t*)(&(out_event_guids[1])));
        fork_task_prescriber(largestN, n-2, cutoff, (ocrGuid_t*)(&(out_db_guids[nNodes(n-cutoff-1)+1])), (ocrGuid_t*)(&(out_event_guids[nNodes(n-cutoff-1)+1])));
        join_task_prescriber(largestN, n , cutoff, (ocrGuid_t*) out_db_guids, (ocrGuid_t*) out_event_guids);
    } else {
        void* temp_db;
        ocrDbCreate( &(out_db_guids[0]), &temp_db, sizeof(u64_t), FLAGS, NULL, NO_ALLOC );
        *((u64_t*)temp_db) = inefficientSerialFib(n);
        ocrEventSatisfy(out_event_guids[0], out_db_guids[0]);
    }
    free(paramv);
}

void fork_task_prescriber ( int largestN, int n, int cutoff, ocrGuid_t* out_db_guids, ocrGuid_t* out_event_guids ) { 
    u64_t *func_args = (u64_t *)malloc(5*sizeof(u64_t));
    func_args[0] = n;
    func_args[1] = cutoff;
    func_args[2] = (u64_t) out_db_guids;
    func_args[3] = (u64_t) out_event_guids;
    func_args[4] = (u64_t) largestN;

    long double nDescendants = computeNDescendants(n-cutoff, largestN);
    long double priority = nDescendants;
    ocrGuid_t task_guid;
    ocrEdtCreate(&task_guid, fibonacci_fork_task, 5, NULL, (void**)func_args, PROPERTIES, 0, NULL, nDescendants, priority);

    ocrEdtSchedule(task_guid);
}

#ifdef VERIFY
typedef struct _userOcrDataBlock {
    ocrGuid_t a;
    void (*create)(struct _userOcrDataBlock *self,                                
    ocrGuid_t allocator,                                         
    u64 size, u16 flags, void* configuration);                   
  
    void (*destruct)(struct _userOcrDataBlock *self);                             
    void* (*acquire)(struct _userOcrDataBlock *self, ocrGuid_t edt, bool isInternal);
    u8 (*release)(struct _userOcrDataBlock *self, ocrGuid_t edt, bool isInternal);
    u8 (*free)(struct _userOcrDataBlock *self, ocrGuid_t edt);                    
} userOcrDataBlock_t;
typedef struct _userOcrPlaceTracker { u64 a; } userOcrPlaceTracker_t;

typedef struct _userOcrDataBlockSimplest {                                        
    userOcrDataBlock_t base;                                                        
    void* ptr;
    ocrGuid_t allocatorGuid; 
    u32 size;
    userOcrPlaceTracker_t placeTracker;                                             
} userOcrDataBlockSimplest_t ;   
#endif

u8 wrap_up_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_stop();
#endif
#ifdef PAPI
    instrument_end();
    instrument_print();
#endif

    gettimeofday(&b,0);
    fprintf(stderr,"The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
    fprintf(stderr,"result: %llu\n", *((u64_t*)depv[0].ptr));

    u64_t *func_args = (u64_t*)paramv;
    int n = (int) func_args[0];
    int cutoff = (int) func_args[1];
    ocrGuid_t* out_db_guids = (ocrGuid_t*) func_args[2];
    ocrGuid_t* out_event_guids = (ocrGuid_t*) func_args[3];

    fprintf(stderr,"dynamic result: %llu\n", efficientSerialFib(n));
    ocrFinish();

#ifdef VERIFY
    int i;
    int totalNodes = nNodes(n-cutoff);
    for ( i = 0; i < totalNodes; ++i) {
        fprintf(stderr, "buffer: %llu\n", *(u64_t*)(((userOcrDataBlockSimplest_t*)(out_db_guids[i]))->ptr));
    }
#endif

    free(paramv);
}

void wrap_up_task_prescriber ( int n, int cutoff, ocrGuid_t* out_db_guids, ocrGuid_t* out_event_guids ) {
    u64_t *func_args = (u64_t *)malloc(4*sizeof(u64_t));
    func_args[0] = n;
    func_args[1] = cutoff;
    func_args[2] = (u64_t) out_db_guids;
    func_args[3] = (u64_t) out_event_guids;

    long double nDescendants = 0;
    long double priority = 0; 
    ocrGuid_t task_guid;
    ocrEdtCreate(&task_guid, wrap_up_task, 4, NULL, (void**)func_args, PROPERTIES, 1, NULL, nDescendants, priority);

    ocrAddDependence(out_event_guids[0], task_guid, 0);
    ocrEdtSchedule(task_guid);
}

inline static ocrGuid_t* allocateCreateEvents ( int n, int cutoff ) {
    int i,j,k;
    int totalNodes = nNodes(n-cutoff);
    ocrGuid_t* event_guids = (ocrGuid_t *) malloc(sizeof(ocrGuid_t)*totalNodes);
    for( i = 0 ; i < totalNodes; ++i ) {
        ocrEventCreate(&(event_guids[i]), OCR_EVENT_STICKY_T, true);
    }

    return event_guids;
}

inline static void freeEvents ( ocrGuid_t* event_guids, int n, int cutoff) {
    int i;
    int totalNodes = nNodes(n-cutoff);
    for( i = 0 ; i < totalNodes; ++i ) {
        ocrEventDestroy(event_guids[i]);
    }
    free(event_guids);
}

inline static ocrGuid_t* allocateDataBlockGuids ( int n, int cutoff) {
    int i;
    int totalNodes = nNodes(n-cutoff);
    ocrGuid_t* db_guids = (ocrGuid_t *) malloc(sizeof(ocrGuid_t)*totalNodes);
    return db_guids;
}

inline static void freeDataBlocks ( ocrGuid_t* db_guids, int n, int cutoff) {
    int i;
    int totalNodes = nNodes(n-cutoff);
    for( i = 0 ; i < totalNodes; ++i ) {
        ocrDbDestroy(db_guids[i]);
    }
    free(db_guids);
}

void assertParameters ( int argc ) {
    if ( argc !=  8 ) {
        fprintf(stderr, "Usage: ./fibonacci -md <> -bind <> N cutoff repeat (found %d args)\n", argc);
        assert(0 && "failed runtime parameters list");
        exit(0);
    }
}

void assertParameterValues ( int n, int cutoff ) {
    if ( n < cutoff ) {
        fprintf(stderr, "n:%d has to be greater than cutoff:%d\n", n, cutoff);
        assert(0 && "failed runtime parameters");
        exit(0);
    }
    if ( cutoff < 1 ) {
        fprintf(stderr, "cutoff:%d has to be at least 1\n", cutoff);
        assert(0 && "failed runtime parameters");
        exit(0);
    }
}

int main( int argc, char* argv[] ) {
    assertParameters(argc);
    int n = -1;
    int cutoff = -1;
    int repeat = -1;

    n = atoi(argv[5]);
    cutoff = atoi(argv[6]);
    repeat = atoi(argv[7]);

    assertParameterValues(n,cutoff);

    int iteration = 0;
    for ( iteration = 0; iteration < repeat; ++iteration) {
    int changing_argc = argc;
    char** changing_argv = (char**) malloc(sizeof(char*)*argc);
    int argc_it = 0;
    /*sagnak, this will have to leak memory because of the way shift_arguments are implemented in the runtime :( */
    for ( ; argc_it < argc; ++argc_it ) {
        changing_argv[argc_it] = (char*)malloc(sizeof(char)*strlen(argv[argc_it])+1);
        strcpy(changing_argv[argc_it], argv[argc_it]);
    }
    OCR_INIT(&changing_argc, changing_argv, fibonacci_fork_task, fibonacci_join_task);

    ocrGuid_t*  event_guids = allocateCreateEvents(n, cutoff);
    ocrGuid_t*  db_guids    = allocateDataBlockGuids(n, cutoff);

#ifdef HPCTOOLKIT
    hpctoolkit_sampling_start();
#endif
#ifdef PAPI
    instrument_init();
    instrument_start();
#endif
    gettimeofday(&a,0);

    fork_task_prescriber ( n, n, cutoff, db_guids, event_guids );
    wrap_up_task_prescriber ( n, cutoff, db_guids, event_guids );

    ocrCleanup();
    freeEvents(event_guids, n, cutoff);
    freeDataBlocks(db_guids, n, cutoff);
    unravel();
    }
    return 0;
}
