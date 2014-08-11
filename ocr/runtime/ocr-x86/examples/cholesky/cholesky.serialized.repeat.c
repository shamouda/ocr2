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

#include "math.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <stdint.h>
#ifdef HPCTOOLKIT
#include <hpctoolkit.h>
#endif

#define FLAGS 0xdead
#define PROPERTIES 0xdead

#define ddf_t void*
typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned char u8;
typedef struct ocrEdtDep_t { void* ptr; u64 guid; } ocrEdtDep_t;
//sagnak: CAUTION:these are just relevant within tiles
#ifndef ALTERNATE_INDEXING
#define TILE_INDEX_2D(tile,x,y) ( tile[y][x] ) 
#define INDEX_1D(x,y) ( (y) * tileSize + x ) 
#else
#define TILE_INDEX_2D(tile,x,y) ( tile[x][y] ) 
#define INDEX_1D(x,y) ( (x) * tileSize + y ) 
#endif

struct timeval a,b;

u8 sequential_cholesky_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index = 0, iB = 0, jB = 0, kB = 0, jBB = 0;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int tileSize = (int) func_args[1];
    ddf_t* out_lkji_kkkp1_event_guid = (ddf_t*) func_args[2];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    void* lBlock_db	= malloc(sizeof(double)*tileSize*tileSize);

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

    *out_lkji_kkkp1_event_guid = lBlock_db;

    free(lBlock2D);
    free(aBlock2D);

    free(paramv);
    free(depv);
}

u8 trisolve_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index, iB, jB, kB;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int tileSize = (int) func_args[2];
    ddf_t* out_lkji_jkkp1_event_guid = (ddf_t*)func_args[3];

    double* aBlock = (double*) (depv[0].ptr);
    double** aBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        aBlock2D[index] = &(aBlock[index*tileSize]);

    double* liBlock = (double*) (depv[1].ptr);
    double** liBlock2D = (double**) malloc(sizeof(double*)*tileSize);
    for( index = 0; index < tileSize; ++index )
        liBlock2D[index] = &(liBlock[index*tileSize]);

    void* loBlock_db = malloc(sizeof(double)*tileSize*tileSize);

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

    *out_lkji_jkkp1_event_guid = loBlock_db;

    free(loBlock2D);
    free(liBlock2D);
    free(aBlock2D);

    free(paramv);
    free(depv);
}

u8 update_diagonal_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int index, iB, jB, kB;
    double temp = 0;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int i = (int) func_args[2];
    int tileSize = (int) func_args[3];
    ddf_t* out_lkji_jjkp1_event_guid = (ddf_t*)func_args[4];

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

    *out_lkji_jjkp1_event_guid = depv[0].ptr;

    free(l2Block2D);
    free(aBlock2D);

    free(paramv);
    free(depv);
}

u8 update_nondiagonal_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    double temp;
    int index, jB, kB, iB;

    intptr_t *func_args = (intptr_t*)paramv;
    int k = (int) func_args[0];
    int j = (int) func_args[1];
    int i = (int) func_args[2];
    int tileSize = (int) func_args[3];
    ddf_t* out_lkji_jikp1_event_guid = (ddf_t*)func_args[4];

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

    *out_lkji_jikp1_event_guid = depv[0].ptr;

    free(l2Block2D);
    free(l1Block2D);
    free(aBlock2D);

    free(paramv);
    free(depv);
}

u8 wrap_up_task ( u32 paramc, u64 * params, void* paramv[], u32 depc, ocrEdtDep_t depv[]) {
    int i, j, i_b, j_b;
    double* temp;
    gettimeofday(&b,0);
    printf("The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_stop();
#endif

    intptr_t *func_args = (intptr_t*)paramv;
    int numTiles = (int) func_args[0];
    int tileSize = (int) func_args[1];
    int times = (int) func_args[2];

#ifdef VERIFY
    char filename[16];
    strcpy(filename, "cholesky.out.");
    snprintf(&(filename[13]), 2, "%d", times);
    filename[15] = '\0';
    FILE* out = fopen(filename, "w");

    for ( i = 0; i < numTiles; ++i ) {
        for( i_b = 0; i_b < tileSize; ++i_b) {
            for ( j = 0; j <= i; ++j ) {
                temp = (double*) (depv[i*(i+1)/2+j].ptr);
                if(i != j) {
                    for(j_b = 0; j_b < tileSize; ++j_b) {
                        fprintf( out, "%lf ", temp[ INDEX_1D(i_b,j_b) ]);
                    }
                } else {
                    for(j_b = 0; j_b <= i_b; ++j_b) {
                        fprintf( out, "%lf ", temp[ INDEX_1D(i_b,j_b) ]);
                    }
                }
            }
        }
    }
#endif

    free(paramv);
    free(depv);
}

inline static void sequential_cholesky_task_prescriber ( int k, int tileSize, ddf_t*** lkji_event_guids) {
    intptr_t *func_args = (intptr_t *)malloc(3*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = tileSize;
    func_args[2] = (intptr_t)&(lkji_event_guids[k][k][k+1]);

    ocrEdtDep_t* deps = (ocrEdtDep_t*) malloc(sizeof(ocrEdtDep_t));
    deps[0].ptr = lkji_event_guids[k][k][k];
    sequential_cholesky_task ( 3, NULL, (void**)func_args, 1, deps );
}

inline static void trisolve_task_prescriber ( int k, int j, int tileSize, ddf_t*** lkji_event_guids) {
    intptr_t *func_args = (intptr_t *)malloc(4*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = tileSize;
    func_args[3] = (intptr_t)&(lkji_event_guids[j][k][k+1]);

    ocrEdtDep_t* deps = (ocrEdtDep_t*) malloc(2*sizeof(ocrEdtDep_t));
    deps[0].ptr = lkji_event_guids[j][k][k];
    deps[1].ptr = lkji_event_guids[k][k][k+1];
    trisolve_task(4, NULL, (void**)func_args, 2, deps);
}

inline static void update_nondiagonal_task_prescriber ( int k, int j, int i, int tileSize, ddf_t*** lkji_event_guids) { 
    intptr_t *func_args = (intptr_t *)malloc(5*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = i;
    func_args[3] = tileSize;
    func_args[4] = (intptr_t)&(lkji_event_guids[j][i][k+1]);

    ocrEdtDep_t* deps = (ocrEdtDep_t*) malloc(3*sizeof(ocrEdtDep_t));
    deps[0].ptr = lkji_event_guids[j][i][k];
    deps[1].ptr = lkji_event_guids[j][k][k+1];
    deps[2].ptr = lkji_event_guids[i][k][k+1];
    update_nondiagonal_task(5, NULL, (void**)func_args, 3, deps);
}


inline static void update_diagonal_task_prescriber ( int k, int j, int i, int tileSize, ddf_t*** lkji_event_guids) { 
    intptr_t *func_args = (intptr_t *)malloc(5*sizeof(intptr_t));
    func_args[0] = k;
    func_args[1] = j;
    func_args[2] = i;
    func_args[3] = tileSize;
    func_args[4] = (intptr_t)&(lkji_event_guids[j][j][k+1]);

    ocrEdtDep_t* deps = (ocrEdtDep_t*) malloc(2*sizeof(ocrEdtDep_t));
    deps[0].ptr = lkji_event_guids[j][j][k];
    deps[1].ptr = lkji_event_guids[j][k][k+1];

    update_diagonal_task (5, NULL, (void**)func_args, 2, deps);
}

inline static void wrap_up_task_prescriber ( int numTiles, int tileSize, ddf_t*** lkji_event_guids, int times ) {
    int i,j,k;

    intptr_t *func_args = (intptr_t *)malloc(3*sizeof(intptr_t));
    func_args[0]=(int)numTiles;
    func_args[1]=(int)tileSize;
    func_args[2]=(int)times;

    ocrEdtDep_t* deps = (ocrEdtDep_t*) malloc((numTiles+1)*numTiles/2*sizeof(ocrEdtDep_t));

    int index = 0;
    for ( i = 0; i < numTiles; ++i ) {
        k = 1;
        for ( j = 0; j <= i; ++j ) {
            deps[index++].ptr = lkji_event_guids[i][j][k];
            ++k;
        }
    }
    wrap_up_task (3, NULL, (void**)func_args, (numTiles+1)*numTiles/2, deps);
}

inline static ddf_t*** allocateCreateEvents ( int numTiles ) {
    int i,j,k;
    ddf_t*** lkji_event_guids;

    lkji_event_guids = (ddf_t ***) malloc(sizeof(ddf_t **)*numTiles);
    for( i = 0 ; i < numTiles ; ++i ) {
        lkji_event_guids[i] = (ddf_t **) malloc(sizeof(ddf_t *)*(i+1));
        for( j = 0 ; j <= i ; ++j ) {
            lkji_event_guids[i][j] = (ddf_t *) malloc(sizeof(ddf_t)*(numTiles+1));
        }
    }

    return lkji_event_guids;
}

inline static void freeEvents ( ddf_t*** lkji_event_guids, int numTiles ) {
    int i,j;
    for( i = 0 ; i < numTiles ; ++i ) {
        for( j = 0 ; j <= i ; ++j ) {
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

inline static freeMatrix ( double** A, int matrixSize ) {
    int i;
    for( i = 0; i < matrixSize; ++i)
        free(A[i]);
    free(A);
}

inline static void satisfyInitialTiles( int numTiles, int tileSize, double** matrix, ddf_t*** lkji_event_guids) {
    int i,j,index;
    int A_i, A_j, T_i, T_j;

    for( i = 0 ; i < numTiles ; ++i ) {
        for( j = 0 ; j <= i ; ++j ) {
            void* temp_db = malloc(sizeof(double)*tileSize*tileSize);
            double* temp = (double*) temp_db;
            double** temp2D = (double**) malloc(sizeof(double*)*tileSize);

            for( index = 0; index < tileSize; ++index )
                temp2D [index] = &(temp[index*tileSize]);

            // Split the matrix into tiles and write it into the item space at time 0.
            // The tiles are indexed by tile indices (which are tag values).
            for( A_i = i*tileSize, T_i = 0 ; T_i < tileSize; ++A_i, ++T_i ) {
                for( A_j = j*tileSize, T_j = 0 ; T_j < tileSize; ++A_j, ++T_j ) {
                    TILE_INDEX_2D(temp2D, T_i, T_j) = matrix[ A_i ][ A_j ];
                }
            }
            lkji_event_guids[i][j][0] = temp_db;
            free(temp2D);
        }
    }
}

int main( int argc, char* argv[] ) {
    int i, j, k;
    double **matrix, ** temp;
    FILE *in;

    int matrixSize = -1;
    int tileSize = -1;
    int numTiles = -1;

    if ( argc !=  5 ) {
        printf("Usage: ./cholesky matrixSize tileSize fileName times (found %d args)\n", argc);
        return 1;
    }

    matrixSize = atoi(argv[1]);
    tileSize = atoi(argv[2]);

    if ( matrixSize % tileSize != 0 ) {
        printf("Incorrect tile size %d for the matrix of size %d \n", tileSize, matrixSize);
        return 1;
    }

    in = fopen(argv[3], "r");
    if( !in ) {
        printf("Cannot find file: %s\n", argv[3]);
        return 1;
    }

    numTiles = matrixSize/tileSize;

    matrix = readMatrix( matrixSize, in );

    fclose(in);

    int nTimes = atoi(argv[4]);
    int times = 0;
    for ( ; times < nTimes; ++times ) {
        ddf_t*** lkji_event_guids = allocateCreateEvents(numTiles);

        satisfyInitialTiles( numTiles, tileSize, matrix, lkji_event_guids);

#ifdef HPCTOOLKIT
        hpctoolkit_sampling_start();
#endif
        gettimeofday(&a,0);

        for ( k = 0; k < numTiles; ++k ) {
            sequential_cholesky_task_prescriber ( k, tileSize, lkji_event_guids);

            for( j = k + 1 ; j < numTiles ; ++j ) {
                trisolve_task_prescriber ( k, j, tileSize, lkji_event_guids);

                update_diagonal_task_prescriber ( k, j, i, tileSize, lkji_event_guids);
                for( i = k + 1 ; i < j ; ++i ) {
                    update_nondiagonal_task_prescriber ( k, j, i, tileSize, lkji_event_guids);
                }
            }
        }

        wrap_up_task_prescriber ( numTiles, tileSize, lkji_event_guids, times );
        freeEvents(lkji_event_guids, numTiles);
    }
    freeMatrix(matrix, matrixSize);
    return 0;
}
