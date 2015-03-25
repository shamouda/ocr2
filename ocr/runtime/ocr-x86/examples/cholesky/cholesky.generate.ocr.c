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

#include "mkl.h"
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

//sagnak: CAUTION:these are just relevant within tiles
#ifndef ALTERNATE_INDEXING
#define TILE_INDEX_2D(tile,x,y) ( tile[y][x] ) 
#define INDEX_1D(x,y) ( (y) * tileSize + x ) 
#else
#define TILE_INDEX_2D(tile,x,y) ( tile[x][y] ) 
#define INDEX_1D(x,y) ( (x) * tileSize + y ) 
#endif

struct timeval a,b,c,d;

#define SEED 1000

u8 sequential_cholesky_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index = 0, iB = 0, jB = 0, kB = 0, jBB = 0;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int tileSize = (int) func_args[1];
    ocrGuid_t out_lkji_kkkp1_event_guid = (ocrGuid_t) func_args[2];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    void* lBlock_db;
    ocrGuid_t out_lkji_kkkp1_db_guid;
    ocrDbCreate( &out_lkji_kkkp1_db_guid, &lBlock_db, sizeof(double)*tileSize*tileSize, FLAGS, NULL, NO_ALLOC );

    double* lBlock = (double*) lBlock_db;
    double** lBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        lBlock2D[index] = &(lBlock[index*tileSize]);

    for( kB = 0 ; kB < tileSize ; ++kB ) {
        if( TILE_INDEX_2D(aBlock2D, kB, kB) <= 0 ) {
            fprintf(stderr,"Not a symmetric positive definite (SPD) matrix\n"); exit(1);
        } else {
            TILE_INDEX_2D(lBlock2D, kB, kB) = sqrt( TILE_INDEX_2D(aBlock2D, kB ,kB) );
        }

        for(jB = kB + 1; jB < tileSize ; ++jB )
            TILE_INDEX_2D(lBlock2D, jB ,kB) = TILE_INDEX_2D(aBlock2D, jB ,kB) / TILE_INDEX_2D(lBlock2D, kB ,kB);

        for(jBB= kB + 1; jBB < tileSize ; ++jBB )
            for(iB = jBB ; iB < tileSize ; ++iB )
                TILE_INDEX_2D(aBlock2D, iB, jBB) -= TILE_INDEX_2D(lBlock2D, iB ,kB) * TILE_INDEX_2D(lBlock2D, jBB, kB);
    }

    ocrEventSatisfy(out_lkji_kkkp1_event_guid, out_lkji_kkkp1_db_guid);

    free(lBlock2D);
    free(aBlock2D);

    free(paramv);
}

u8 trisolve_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index, iB, jB, kB;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int tileSize = (int) func_args[2];
    ocrGuid_t out_lkji_jkkp1_event_guid = (ocrGuid_t) func_args[3];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    double* liBlock = (double*) (depv[1].ptr);
    double** liBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        liBlock2D[index] = &(liBlock[index*tileSize]);

    ocrGuid_t out_lkji_jkkp1_db_guid;
    void* loBlock_db;
    ocrDbCreate( &out_lkji_jkkp1_db_guid, &loBlock_db, sizeof(double)*tileSize*tileSize, FLAGS, NULL, NO_ALLOC);

    double * loBlock = (double*) loBlock_db;
    double** loBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        loBlock2D[index] = &(loBlock[index*tileSize]);

    for( kB = 0; kB < tileSize ; ++kB ) {
        for( iB = 0; iB < tileSize ; ++iB )
            TILE_INDEX_2D(loBlock2D, iB, kB) = TILE_INDEX_2D(aBlock2D,iB,kB) / TILE_INDEX_2D(liBlock2D, kB, kB);

        for( jB = kB + 1 ; jB < tileSize; ++jB )
            for( iB = 0; iB < tileSize; ++iB )
                TILE_INDEX_2D(aBlock2D, iB, jB) -= TILE_INDEX_2D(liBlock2D, jB, kB) * TILE_INDEX_2D(loBlock2D, iB, kB);
    }

    ocrEventSatisfy(out_lkji_jkkp1_event_guid, out_lkji_jkkp1_db_guid);

    free(loBlock2D);
    free(liBlock2D);
    free(aBlock2D);

    free(paramv);
}

u8 update_diagonal_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index, iB, jB, kB;
    double temp = 0;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int i = (int) func_args[2];
    int tileSize = (int) func_args[3];
    ocrGuid_t out_lkji_jjkp1_event_guid = (ocrGuid_t) func_args[4];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    double* l2Block = (double*) (depv[1].ptr);
    double** l2Block2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        l2Block2D[index] = &(l2Block[index*tileSize]);

    for( jB = 0; jB < tileSize ; ++jB ) {
        for( kB = 0; kB < tileSize ; ++kB ) {
            temp = 0 - TILE_INDEX_2D(l2Block2D, jB, kB);
            for( iB = jB; iB < tileSize; ++iB )
                TILE_INDEX_2D(aBlock2D, iB, jB) += temp * TILE_INDEX_2D(l2Block2D, iB, kB);
        }
    }

    ocrEventSatisfy(out_lkji_jjkp1_event_guid, depv[0].guid);

    free(l2Block2D);
    free(aBlock2D);

    free(paramv);
}

u8 update_nondiagonal_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    double temp;
    int index, jB, kB, iB;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int i = (int) func_args[2];
    int tileSize = (int) func_args[3];
    ocrGuid_t out_lkji_jikp1_event_guid = (ocrGuid_t) func_args[4];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    double* l1Block = (double*) (depv[1].ptr);
    double** l1Block2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        l1Block2D[index] = &(l1Block[index*tileSize]);

    double* l2Block = (double*) (depv[2].ptr);
    double** l2Block2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        l2Block2D[index] = &(l2Block[index*tileSize]);

    for( jB = 0; jB < tileSize ; ++jB ) {
        for( kB = 0; kB < tileSize ; ++kB ) {
            temp = 0 - TILE_INDEX_2D(l2Block2D, jB, kB);
            for( iB = 0; iB < tileSize ; ++iB )
                TILE_INDEX_2D(aBlock2D, iB, jB) += temp * TILE_INDEX_2D(l1Block2D, iB, kB);
        }
    }

    ocrEventSatisfy(out_lkji_jikp1_event_guid, depv[0].guid);

    free(l2Block2D);
    free(l1Block2D);
    free(aBlock2D);

    free(paramv);
}

u8 wrap_up_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int i, j, i_b, j_b;
    double* temp;
    gettimeofday(&b,0);
    printf("The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_stop();
#endif
#ifdef PAPI
    instrument_end();
    instrument_print();
#endif
    ocrFinish();

    intptr_t *func_args = (intptr_t*)paramv;
    int numTiles = (int) func_args[0];
    int tileSize = (int) func_args[1];
    double* input_solution = (double*) func_args[2];

    int matrixSize = numTiles * tileSize;
#ifdef VERIFY
    long double max_diff = -100.0L;

    for ( i = 0; i < numTiles; ++i ) {
        for( i_b = 0; i_b < tileSize; ++i_b) {
            for ( j = 0; j <= i; ++j ) {
                temp = (double*) (depv[i*(i+1)/2+j].ptr);
                if(i != j) {
                    for(j_b = 0; j_b < tileSize; ++j_b) {
                        long double diff = (long double) temp[ INDEX_1D(i_b,j_b) ] - (long double)input_solution[ (i*tileSize+i_b)*matrixSize + (j*tileSize) + j_b]; 
                        if ( diff < 0.0L ) diff = 0.0L - diff;
                        if ( diff > max_diff ) max_diff = diff;
                    }
                } else {
                    for(j_b = 0; j_b <= i_b; ++j_b) {
                        long double diff = (long double)temp[ INDEX_1D(i_b,j_b) ] - (long double)input_solution[ (i*tileSize+i_b)*matrixSize + (j*tileSize) + j_b]; 
                        if ( diff < 0.0L ) diff = 0.0L - diff;
                        if ( diff > max_diff ) max_diff = diff;
                    }
                }
            }
        }
    }

    printf("epsilon: %Lf\n", max_diff);
#endif

    free(paramv);
}

static inline void myCheapPower( long double* p_big, int powerOf2 ) {
    if ( powerOf2 < 64 ) {
        *p_big = 1LLU << powerOf2;
    } else {
        myCheapPower(p_big, powerOf2-63);
        *p_big *= 1LLU << 63;
    }
}

static inline long double factorCalculator ( int x ) {
    long double fluff = -9*x*x+3*x+43;
    long double big;
    myCheapPower(&big,(2*x+1));
    long double bigger = fluff + big * 19; 

    return (bigger)/27;
}

static inline long double dpotrfCountCalculator ( int numTiles, int k ) {
    return factorCalculator(numTiles-k-1)-(numTiles-k)-2;
}

static inline long double dtrsmCountCalculator ( int numTiles, int k, int j ) {
    long double big;
    myCheapPower(&big,j-k);
    return factorCalculator(numTiles-j-1)*big-(numTiles-k)-2;
}

static inline long double dsyrkCountCalculator ( int numTiles, int k, int j ) {
    return factorCalculator(numTiles-j-1)-(numTiles-j)-2+(j-k);
}

static inline long double dgemmCountCalculator ( int numTiles, int k, int j, int i ) {
    long double big;
    myCheapPower(&big,j-i);
    return factorCalculator(numTiles-j-1)*big-numTiles+i-2+i-k;
}

inline static void sequential_cholesky_task_prescriber ( int k, int tileSize, int numTiles, ocrGuid_t*** lkji_event_guids, ocrGuid_t* dpotrf_guids) {
    intptr_t *func_args = (intptr_t *)malloc(3*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = tileSize;
    func_args[2] = (ocrGuid_t)lkji_event_guids[k][k][k+1];

    long double nDescendants = dpotrfCountCalculator(numTiles, k);
    long double priority = nDescendants;
    ocrEdtCreate(&(dpotrf_guids[k]), sequential_cholesky_task, 3, NULL, (void**)func_args, PROPERTIES, 1, NULL, nDescendants, priority);

    ocrAddDependence(lkji_event_guids[k][k][k], dpotrf_guids[k], 0);
    ocrEdtSchedule(dpotrf_guids[k]);
}

inline static void trisolve_task_prescriber ( int k, int j, int tileSize, int numTiles, ocrGuid_t*** lkji_event_guids, ocrGuid_t** dtrsm_guids) {
    intptr_t *func_args = (intptr_t *)malloc(4*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = tileSize;
    func_args[3] = (ocrGuid_t)lkji_event_guids[j][k][k+1];

    long double nDescendants = dtrsmCountCalculator(numTiles, k, j);
    long double priority = nDescendants;
    ocrEdtCreate(&(dtrsm_guids[k][j-k-1]), trisolve_task, 4, NULL, (void**)func_args, PROPERTIES, 2, NULL, nDescendants, priority);

    ocrAddDependence(lkji_event_guids[j][k][k], dtrsm_guids[k][j-k-1], 0);
    ocrAddDependence(lkji_event_guids[k][k][k+1], dtrsm_guids[k][j-k-1], 1);
    ocrEdtSchedule(dtrsm_guids[k][j-k-1]);
}

inline static void update_nondiagonal_task_prescriber ( int k, int j, int i, int tileSize, int numTiles, ocrGuid_t*** lkji_event_guids, ocrGuid_t*** dgemm_guids) { 
    intptr_t *func_args = (intptr_t *)malloc(5*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = i;
    func_args[3] = tileSize;
    func_args[4] = (ocrGuid_t)lkji_event_guids[j][i][k+1];

    long double nDescendants = dgemmCountCalculator(numTiles, k, j, i);
    long double priority = nDescendants;
    ocrEdtCreate(&(dgemm_guids[k][j-k-1][i-k-1]), update_nondiagonal_task, 5, NULL, (void**)func_args, PROPERTIES, 3, NULL, nDescendants, priority);

    ocrAddDependence(lkji_event_guids[j][i][k], dgemm_guids[k][j-k-1][i-k-1], 0);
    ocrAddDependence(lkji_event_guids[j][k][k+1], dgemm_guids[k][j-k-1][i-k-1], 1);
    ocrAddDependence(lkji_event_guids[i][k][k+1], dgemm_guids[k][j-k-1][i-k-1], 2);

    ocrEdtSchedule(dgemm_guids[k][j-k-1][i-k-1]);
}


inline static void update_diagonal_task_prescriber ( int k, int j, int i, int tileSize, int numTiles, ocrGuid_t*** lkji_event_guids, ocrGuid_t** dsyrk_guids) { 
    intptr_t *func_args = (intptr_t *)malloc(5*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = i;
    func_args[3] = tileSize;
    func_args[4] = (ocrGuid_t)lkji_event_guids[j][j][k+1];

    long double nDescendants = dsyrkCountCalculator(numTiles, k, j);
    long double priority = nDescendants;
    ocrEdtCreate(&(dsyrk_guids[k][j-k-1]), update_diagonal_task, 5, NULL, (void**)func_args, PROPERTIES, 2, NULL, nDescendants, priority);

    ocrAddDependence(lkji_event_guids[j][j][k], dsyrk_guids[k][j-k-1], 0);
    ocrAddDependence(lkji_event_guids[j][k][k+1], dsyrk_guids[k][j-k-1], 1);

    ocrEdtSchedule(dsyrk_guids[k][j-k-1]);
}

inline static void wrap_up_task_prescriber ( int numTiles, int tileSize, ocrGuid_t*** lkji_event_guids, double* input_solution, ocrGuid_t* task_guid) {
    int i,j,k;

    intptr_t *func_args = (intptr_t *)malloc(3*sizeof(intptr_t));
    func_args[0]=(intptr_t)numTiles;
    func_args[1]=(intptr_t)tileSize;
    func_args[2]=(intptr_t)input_solution;

    ocrEdtCreate(task_guid, wrap_up_task, 3, NULL, (void**)func_args, PROPERTIES, (numTiles+1)*numTiles/2, NULL, 0, 0);

    int index = 0;
    for ( i = 0; i < numTiles; ++i ) {
        k = 1;
        for ( j = 0; j <= i; ++j ) {
            ocrAddDependence(lkji_event_guids[i][j][k], *task_guid, index++);
            ++k;
        }
    }

    ocrEdtSchedule(*task_guid);
}

inline static ocrGuid_t*** allocateCreateEvents ( int numTiles ) {
    int i,j,k;
    ocrGuid_t*** lkji_event_guids;

    lkji_event_guids = (ocrGuid_t ***) malloc(sizeof(ocrGuid_t **)*numTiles);
    for( i = 0 ; i < numTiles ; ++i ) {
        lkji_event_guids[i] = (ocrGuid_t **) malloc(sizeof(ocrGuid_t *)*(i+1));
        for( j = 0 ; j <= i ; ++j ) {
            lkji_event_guids[i][j] = (ocrGuid_t *) malloc(sizeof(ocrGuid_t)*(numTiles+1));
            for( k = 0 ; k <= numTiles ; ++k )
                ocrEventCreate(&(lkji_event_guids[i][j][k]), OCR_EVENT_STICKY_T, true);
        }
    }

    return lkji_event_guids;
}

inline static void freeEvents ( ocrGuid_t*** lkji_event_guids, int numTiles ) {
    int i,j,k;
    for( i = 0 ; i < numTiles ; ++i ) {
        for( j = 0 ; j <= i ; ++j ) {
            for( k = 0 ; k <= numTiles ; ++k )
                ocrEventDestroy(lkji_event_guids[i][j][k]);
            free(lkji_event_guids[i][j]);
        }
        free(lkji_event_guids[i]);
    }
    free(lkji_event_guids);
}

inline static double** readMatrix( int matrixSize, FILE* in ) {
    int i,j;
    double **A = (double**) malloc (sizeof(double*)*matrixSize);

    for( i = 0; i < matrixSize; ++i)
        A[i] = (double*) malloc(sizeof(double)*matrixSize);

    for( i = 0; i < matrixSize; ++i ) {
        for( j = 0; j < matrixSize-1; ++j )
            fscanf(in, "%lf ", &A[i][j]);
        fscanf(in, "%lf\n", &A[i][j]);
    }
    return A;
}

inline static void createMatrix( double** matrix, double **input_solution, int matrixSize ) {
    gettimeofday(&c,0);
    *input_solution = (double*) malloc (sizeof(double)*(matrixSize*matrixSize+matrixSize));
    *matrix = (double*) malloc (sizeof(double)*matrixSize*matrixSize);

    double *L = *input_solution;
    double *A = *matrix;

    int index,j,k;
    srand(SEED);
    for ( j = 0; j < matrixSize; ++j ) {
        for( k = 0; k < matrixSize; ++k ) {
            index = j*matrixSize + k;
            if ( k < j ) {
                L[index] = ((double)rand()/(double)RAND_MAX);
                L[index] = (L[index] * 2.0) - 1.0;
                L[index] = L[index] / ((double)matrixSize);
            } else if (k == j) {
                L[index] = 1.0;
            }
        }
    }

    static const double alpha = 1.0;
    static const double beta = 0.0;
    dgemm ("T", "N", &matrixSize, &matrixSize, &matrixSize, &alpha, L, &matrixSize, L, &matrixSize, &beta, A, &matrixSize);
    gettimeofday(&d,0);
    printf("creation took %f seconds\r\n",((d.tv_sec - c.tv_sec)*1000000+(d.tv_usec - c.tv_usec))*1.0/1000000);
    char* n_threads_str = getenv("MY_MKL_NUM_THREADS");
    int n_threads = 1;
    if ( NULL != n_threads_str ) n_threads = atoi(n_threads_str);
    mkl_set_num_threads(n_threads);
}

inline static freeMatrix ( double* A, double *L ) {
    int i;
    free(A);
    free(L);
}

inline static ocrGuid_t** allocateDataBlockGuids ( int numTiles ) {
    int i,j,k;
    ocrGuid_t** db_guids;

    db_guids = (ocrGuid_t **) malloc(sizeof(ocrGuid_t *)*numTiles);
    for( i = 0 ; i < numTiles ; ++i ) {
        db_guids[i] = (ocrGuid_t *) malloc(sizeof(ocrGuid_t)*(i+1));
    }

    return db_guids;
}

inline static ocrGuid_t* allocateDpotrfGuids ( int numTiles ) {
    return (ocrGuid_t *) malloc(sizeof(ocrGuid_t)*numTiles);
}

inline static ocrGuid_t** allocateDtrsmGuids ( int numTiles ) {
    int k;
    ocrGuid_t** dtrsm_guids = (ocrGuid_t**)malloc(numTiles*sizeof(ocrGuid_t*));
    for( k = 0 ; k < numTiles ; ++k ) {
        dtrsm_guids[k] = (ocrGuid_t*)malloc((numTiles-1-k)*sizeof(ocrGuid_t));
    }
    return dtrsm_guids;
}

inline static ocrGuid_t** allocateDsyrkGuids ( int numTiles ) {
    int k;
    ocrGuid_t** dsyrk_guids = (ocrGuid_t**)malloc(numTiles*sizeof(ocrGuid_t*));
    for( k = 0 ; k < numTiles ; ++k ) {
        dsyrk_guids[k] = (ocrGuid_t*)malloc((numTiles-1-k)*sizeof(ocrGuid_t));
    }
    return dsyrk_guids;
}

inline static ocrGuid_t*** allocateDgemmGuids ( int numTiles ) {
    int k,j;
    ocrGuid_t*** dgemm_guids = (ocrGuid_t***)malloc(numTiles*sizeof(ocrGuid_t**));
    for( k = 0 ; k < numTiles ; ++k ) {
        dgemm_guids[k] = (ocrGuid_t**)malloc((numTiles-1-k)*sizeof(ocrGuid_t*));
        for( j = k+1 ; j < numTiles ; ++j ) {
            dgemm_guids[k][j-k-1]=(ocrGuid_t*)malloc((j-k-1)*sizeof(ocrGuid_t));
        }
    }
    return dgemm_guids;
}

inline static void freeDpotrfGuids ( ocrGuid_t* dpotrf_guids, int numTiles ) {
    int k;
    for( k = 0 ; k < numTiles; ++k ) {
        ocrEdtDestroy(dpotrf_guids[k]);
    }
    free(dpotrf_guids);
}

inline static void freeDtrsmGuids ( ocrGuid_t** dtrsm_guids, int numTiles ) {
    int k,j;
    for( k = 0 ; k < numTiles; ++k ) {
        for( j = k+1 ; j < numTiles; ++j ) {
            ocrEdtDestroy(dtrsm_guids[k][j-k-1]);
        }
        free(dtrsm_guids[k]);
    }
    free(dtrsm_guids);
}

inline static void freeDsyrkGuids ( ocrGuid_t** dsyrk_guids, int numTiles ) {
    int k,j;
    for( k = 0 ; k < numTiles ; ++k ) {
        for( j = k+1 ; j < numTiles ; ++j ) {
            ocrEdtDestroy(dsyrk_guids[k][j-k-1]);
        }
        free(dsyrk_guids[k]);
    }
    free(dsyrk_guids);
}

inline static void freeDgemmGuids ( ocrGuid_t*** dgemm_guids, int numTiles ) {
    int k,j,i;
    for( k = 0 ; k < numTiles ; ++k ) {
        for( j = k+1 ; j < numTiles ; ++j ) {
            for( i = k+1 ; i < j; ++i ) {
                ocrEdtDestroy(dgemm_guids[k][j-k-1][i-k-1]);
            }
            free(dgemm_guids[k][j-k-1]);
        }
        free(dgemm_guids[k]);
    }
    free(dgemm_guids);
}

inline static void freeDataBlocks ( ocrGuid_t** db_guids, int numTiles ) {
    int i,j,k;
    for( i = 0 ; i < numTiles ; ++i ) {
        for( j = 0 ; j <= i ; ++j ) {
            ocrDbDestroy(db_guids[i][j]);
        }
        free(db_guids[i]);
    }
    free(db_guids);
}

inline static void satisfyInitialTiles( int numTiles, int tileSize, double* matrix, ocrGuid_t*** lkji_event_guids, ocrGuid_t** db_guids) {
    int i,j,index;
    int A_i, A_j, T_i, T_j;
    int matrixSize = numTiles * tileSize;

    for( i = 0 ; i < numTiles ; ++i ) {
        for( j = 0 ; j <= i ; ++j ) {
            void* temp_db;
            ocrDbCreate( &(db_guids[i][j]), &temp_db, sizeof(double)*tileSize*tileSize, FLAGS, NULL, NO_ALLOC );

            double* temp = (double*) temp_db;
            double** temp2D = (double**) malloc(sizeof(double*)*tileSize);
            assert(temp);
            assert(temp2D);

            for( index = 0; index < tileSize; ++index )
                temp2D [index] = &(temp[index*tileSize]);

            // Split the matrix into tiles and write it into the item space at time 0.
            // The tiles are indexed by tile indices (which are tag values).
            for( A_i = i*tileSize, T_i = 0 ; T_i < tileSize; ++A_i, ++T_i ) {
                for( A_j = j*tileSize, T_j = 0 ; T_j < tileSize; ++A_j, ++T_j ) {
                    TILE_INDEX_2D(temp2D, T_i, T_j) = matrix[ A_i * matrixSize + A_j ];
                }
            }
            ocrEventSatisfy(lkji_event_guids[i][j][0], db_guids[i][j]);
            free(temp2D);
        }
    }
}

int main( int argc, char* argv[] ) {
    int i, j, k;
    double *matrix;
    double *input_solution;

    int matrixSize = -1;
    int tileSize = -1;
    int numTiles = -1;

    if ( argc !=  7 ) {
        printf("Usage: ./cholesky -md <> -bind <> matrixSize tileSize (found %d args)\n", argc);
        return 1;
    }

    matrixSize = atoi(argv[5]);
    tileSize = atoi(argv[6]);

    if ( matrixSize % tileSize != 0 ) {
        printf("Incorrect tile size %d for the matrix of size %d \n", tileSize, matrixSize);
        return 1;
    }

    numTiles = matrixSize/tileSize;

    createMatrix ( &matrix, &input_solution, matrixSize );

    int changing_argc = argc;
    char** changing_argv = (char**) malloc(sizeof(char*)*argc);
    int argc_it = 0;
    /*sagnak, this will have to leak memory because of the way shift_arguments are implemented in the runtime :( */
    for ( ; argc_it < argc; ++argc_it ) {
        changing_argv[argc_it] = (char*)malloc(sizeof(char)*strlen(argv[argc_it])+1);
        strcpy(changing_argv[argc_it], argv[argc_it]);
    }
    OCR_INIT(&changing_argc, changing_argv, sequential_cholesky_task, trisolve_task, update_nondiagonal_task, update_diagonal_task, wrap_up_task);

    ocrGuid_t*** lkji_event_guids = allocateCreateEvents(numTiles);
    ocrGuid_t**  db_guids = allocateDataBlockGuids (numTiles);
    ocrGuid_t*   dpotrf_guids = allocateDpotrfGuids (numTiles);
    ocrGuid_t**  dtrsm_guids = allocateDtrsmGuids (numTiles);
    ocrGuid_t**  dsyrk_guids = allocateDsyrkGuids (numTiles);
    ocrGuid_t*** dgemm_guids = allocateDgemmGuids (numTiles);
    ocrGuid_t    wrap_up_task_guid;

    satisfyInitialTiles( numTiles, tileSize, matrix, lkji_event_guids, db_guids);

#ifdef HPCTOOLKIT
    hpctoolkit_sampling_start();
#endif
#ifdef PAPI
    instrument_init();
    instrument_start();
#endif
    gettimeofday(&a,0);

    for ( k = 0; k < numTiles; ++k ) {
        sequential_cholesky_task_prescriber ( k, tileSize, numTiles, lkji_event_guids, dpotrf_guids);

        for( j = k + 1 ; j < numTiles ; ++j ) {
            trisolve_task_prescriber ( k, j, tileSize, numTiles, lkji_event_guids, dtrsm_guids);

            update_diagonal_task_prescriber ( k, j, i, tileSize, numTiles, lkji_event_guids, dsyrk_guids);
            for( i = k + 1 ; i < j ; ++i ) {
                update_nondiagonal_task_prescriber ( k, j, i, tileSize, numTiles, lkji_event_guids, dgemm_guids);
            }
        }
    }

    wrap_up_task_prescriber ( numTiles, tileSize, lkji_event_guids, input_solution, &wrap_up_task_guid);

    ocrCleanup();
    freeEvents(lkji_event_guids, numTiles);
    freeDataBlocks(db_guids, numTiles);
    freeDpotrfGuids(dpotrf_guids,numTiles);
    freeDtrsmGuids(dtrsm_guids, numTiles);
    freeDsyrkGuids(dsyrk_guids, numTiles);
    freeDgemmGuids(dgemm_guids, numTiles);
    ocrEdtDestroy(wrap_up_task_guid);
    unravel();
    freeMatrix(matrix, input_solution);
    return 0;
}