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
#include <assert.h>
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
assert ( 0 && "MKL works only with column-major")
#define TILE_INDEX_2D(tile,x,y) ( tile[x][y] ) 
#define INDEX_1D(x,y) ( (x) * tileSize + y ) 
#endif

struct timeval a,b,c,d;

#define SEED 1000

u8 sequential_cholesky_task ( int k, int tileSize, ddf_t* out_lkji_kkkp1_event_guid, ddf_t in_lkji_kkk_event_guid ) {
    int index = 0, iB = 0, jB = 0, kB = 0, jBB = 0;

    double* aBlock = (double*) (in_lkji_kkk_event_guid);

    int stackTileSize = tileSize;
    static int status = 0;
    dpotrf("L", &stackTileSize, aBlock, &stackTileSize, &status);
    *out_lkji_kkkp1_event_guid = (void*)aBlock;
}

u8 trisolve_task ( int k, int j, int tileSize, ddf_t* out_lkji_jkkp1_event_guid, ddf_t in_lkji_jkk_event_guid, ddf_t in_lkji_kkkp1_event_guid ) {
    int index, iB, jB, kB;

    double*  aBlock = (double*) (in_lkji_jkk_event_guid);
    double* liBlock = (double*) (in_lkji_kkkp1_event_guid);
    static const double alpha = 1.0;

    dtrsm("R", "L", "T", "N", &tileSize, &tileSize, &alpha, liBlock, &tileSize, aBlock, &tileSize);

    *out_lkji_jkkp1_event_guid = (void*)aBlock;
}

u8 update_diagonal_task ( int k, int j, int i, int tileSize, ddf_t* out_lkji_jjkp1_event_guid, ddf_t in_lkji_jjk_event_guid, ddf_t in_lkji_jkkp1_event_guid) {
    int index, iB, jB, kB;
    double temp = 0;

    double* aBlock = (double*) (in_lkji_jjk_event_guid);
    double* l2Block = (double*) (in_lkji_jkkp1_event_guid);

    static const double alpha = -1.0;
    static const double beta = 1.0;
    dsyrk("L", "N", &tileSize, &tileSize, &alpha, l2Block, &tileSize, &beta, aBlock, &tileSize );

    *out_lkji_jjkp1_event_guid = in_lkji_jjk_event_guid;
}

u8 update_nondiagonal_task (int k, int j, int i, int tileSize, ddf_t* out_lkji_jikp1_event_guid , ddf_t in_lkji_jik_event_guid , ddf_t in_lkji_jkkp1_event_guid , ddf_t in_lkji_ikkp1_event_guid ) {
    double temp;
    int index, jB, kB, iB;

    double* aBlock = (double*) in_lkji_jik_event_guid;
    double* l1Block = (double*) in_lkji_jkkp1_event_guid;
    double* l2Block = (double*) in_lkji_ikkp1_event_guid;

    static const double alpha = -1.0;
    static const double beta = 1.0;
    dgemm("N", "T", &tileSize, &tileSize, &tileSize, &alpha, l1Block, &tileSize, l2Block, &tileSize, &beta, aBlock, &tileSize);

    *out_lkji_jikp1_event_guid = in_lkji_jik_event_guid;
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
    double* input_solution = (double*) func_args[2];

    int matrixSize = numTiles * tileSize;
#ifdef VERIFY
    long double max_diff = -100.0;

    for ( i = 0; i < numTiles; ++i ) {
        for( i_b = 0; i_b < tileSize; ++i_b) {
            for ( j = 0; j <= i; ++j ) {
                temp = (double*) (depv[i*(i+1)/2+j].ptr);
                if(i != j) {
                    for(j_b = 0; j_b < tileSize; ++j_b) {
                        long double diff = (long double) temp[ INDEX_1D(i_b,j_b) ] - (long double)input_solution[ (i*tileSize+i_b)*matrixSize + (j*tileSize) + j_b]; 
                        if ( diff < 0.0 ) diff = 0 - diff;
                        if ( diff > max_diff ) max_diff = diff;
                    }
                } else {
                    for(j_b = 0; j_b <= i_b; ++j_b) {
                        long double diff = (long double)temp[ INDEX_1D(i_b,j_b) ] - (long double)input_solution[ (i*tileSize+i_b)*matrixSize + (j*tileSize) + j_b]; 
                        if ( diff < 0.0 ) diff = 0 - diff;
                        if ( diff > max_diff ) max_diff = diff;
                    }
                }
            }
        }
    }

    printf("epsilon: %Lf\n", max_diff);
#endif

    free(paramv);
    free(depv);
}

inline static void wrap_up_task_prescriber ( int numTiles, int tileSize, ddf_t*** lkji_event_guids, double* input_solution ) {
    int i,j,k;

    intptr_t *func_args = (intptr_t *)malloc(3*sizeof(intptr_t));
    func_args[0]=(intptr_t)numTiles;
    func_args[1]=(intptr_t)tileSize;
    func_args[2]=(intptr_t)input_solution;

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

inline static long double*** allocateCreateCounts ( int numTiles ) {
    int i,j,k;
    long double*** descendantCounts;

    descendantCounts = (long double***) malloc(sizeof(long double**)*numTiles);
    for( i = 0 ; i < numTiles ; ++i ) {
        descendantCounts[i] = (long double**) malloc(sizeof(long double*)*(i+1));
        for( j = 0 ; j <= i ; ++j ) {
            descendantCounts[i][j] = (long double*) calloc(numTiles+1, sizeof(long double));
        }
    }

    return descendantCounts;
}

static void update_nondiagonal_count ( int j, int i, int k, long double*** descendantCounts ) {
    const long double self = 1+descendantCounts[j][i][k+1];
    descendantCounts[j][i][k]   += self;
    descendantCounts[j][k][k+1] += self;
    descendantCounts[i][k][k+1] += self;
}

static void update_diagonal_count ( int j, int k, long double*** descendantCounts ) {
     const long double self = 1+descendantCounts[j][j][k+1];
     descendantCounts[j][j][k]   += self;
     descendantCounts[j][k][k+1] += self;
}

static void trisolve_count ( int j, int k, long double*** descendantCounts ) {
    const long double self = 1+descendantCounts[j][k][k+1]; 
    descendantCounts[j][k][k] += self;
    descendantCounts[k][k][k+1] += self;
}

static void sequential_cholesky_count ( int k, long double*** descendantCounts ) {
    const long double self = 1+descendantCounts[k][k][k+1]; 
    descendantCounts[k][k][k] += self;
}

static void countUpdate( long double*** descendantCounts, int numTiles ) {
    int k,j,i;
    for ( k = numTiles - 1; k >= 0; --k ) {
        for( j = numTiles - 1; j >= k + 1; --j ) {
            for( i = j - 1 ; i >= k+1 ; --i ) {
                update_nondiagonal_count (j,i,k,descendantCounts); //lkji[j][i][k], lkji[j][k][k+1], lkji[i][k][k+1]
            }
            update_diagonal_count (j,k,descendantCounts); // lkji[j][j][k], lkji[j][k][k+1]
            trisolve_count (j,k,descendantCounts); // lkji[j][k][k], lkji[k][k][k+1])
        }
        sequential_cholesky_count (k, descendantCounts); // lkji[k][k][k]
    }
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

#define TOLERANCE 0.000001L
static inline int isTolerablyClose (long double a, long double b) {
    if ( a == 0.0L &&  b == 0.0L ) {
         return 1;
    } else {
        long double scaledDiff = (a>b) ? (a-b)/a: (b-a)/b;
        return scaledDiff < TOLERANCE;
    }
    return 0;
}

static void verifyCount( long double*** descendantCounts, int numTiles ) {
    int doesMatch = 0;
    int k,j,i;
    fprintf(stderr, "%Le\n",dpotrfCountCalculator(numTiles,0));
    for ( k = 0; k < numTiles; ++k ) {
        doesMatch = isTolerablyClose(descendantCounts[k][k][k+1],dpotrfCountCalculator(numTiles,k)); 
        if (!doesMatch) fprintf(stderr, "dpotrf[%d][%d][%d] does not verify %Le != %Le \n", k,k,k+1, descendantCounts[k][k][k+1],dpotrfCountCalculator(numTiles,k));

        for( j = k + 1 ; j < numTiles ; ++j ) {
            doesMatch = isTolerablyClose(descendantCounts[j][k][k+1],dtrsmCountCalculator(numTiles,k,j));
            if (!doesMatch) fprintf(stderr, "dtrsm[%d][%d][%d] does not verify %Le != %Le \n", j,k,k+1,descendantCounts[j][k][k+1], dtrsmCountCalculator(numTiles,k,j));

            doesMatch = isTolerablyClose(descendantCounts[j][j][k+1],dsyrkCountCalculator(numTiles,k,j));
            if (!doesMatch) fprintf(stderr, "dsyrk[%d][%d][%d] does not verify %Le != %Le \n", j,j,k+1,descendantCounts[j][j][k+1], dsyrkCountCalculator(numTiles,k,j));

            for( i = k + 1 ; i < j ; ++i ) {
                doesMatch = isTolerablyClose(descendantCounts[j][i][k+1],dgemmCountCalculator(numTiles,k,j,i));
                if (!doesMatch) fprintf(stderr, "dgemm[%d][%d][%d] does not verify %Le != %Le \n", j,i,k+1,descendantCounts[j][i][k+1], dgemmCountCalculator(numTiles,k,j,i));
            }
        }
    }
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

inline static void satisfyInitialTiles( int numTiles, int tileSize, double* matrix, ddf_t*** lkji_event_guids) {
    int i,j,index;
    int A_i, A_j, T_i, T_j;
    int matrixSize = numTiles * tileSize;

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
                    TILE_INDEX_2D(temp2D, T_i, T_j) = matrix[ A_i * matrixSize + A_j ];
                }
            }
            lkji_event_guids[i][j][0] = temp_db;
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

    if ( argc !=  4 ) {
        printf("Usage: ./cholesky matrixSize tileSize times (found %d args)\n", argc);
        return 1;
    }

    matrixSize = atoi(argv[1]);
    tileSize = atoi(argv[2]);

    if ( matrixSize % tileSize != 0 ) {
        printf("Incorrect tile size %d for the matrix of size %d \n", tileSize, matrixSize);
        return 1;
    }

    numTiles = matrixSize/tileSize;

    createMatrix ( &matrix, &input_solution, matrixSize );

    int nTimes = atoi(argv[3]);
    int times = 0;
    for ( ; times < nTimes; ++times ) {
        ddf_t*** lkji_event_guids = allocateCreateEvents(numTiles);
        long double*** descendantCounts = allocateCreateCounts(numTiles);

        satisfyInitialTiles( numTiles, tileSize, matrix, lkji_event_guids);

#ifdef HPCTOOLKIT
        hpctoolkit_sampling_start();
#endif
        gettimeofday(&a,0);

        for ( k = 0; k < numTiles; ++k ) {
            sequential_cholesky_task ( k, tileSize, &lkji_event_guids[k][k][k+1], lkji_event_guids[k][k][k]);

            for( j = k + 1 ; j < numTiles ; ++j ) {
                trisolve_task ( k, j, tileSize, &lkji_event_guids[j][k][k+1], lkji_event_guids[j][k][k], lkji_event_guids[k][k][k+1]);

                update_diagonal_task ( k, j, i, tileSize, &lkji_event_guids[j][j][k+1], lkji_event_guids[j][j][k], lkji_event_guids[j][k][k+1]);

                for( i = k + 1 ; i < j ; ++i ) {
                    update_nondiagonal_task ( k, j, i, tileSize, &lkji_event_guids[j][i][k+1], lkji_event_guids[j][i][k], lkji_event_guids[j][k][k+1], lkji_event_guids[i][k][k+1]);
                }
            }
        }

        wrap_up_task_prescriber ( numTiles, tileSize, lkji_event_guids, input_solution );
        freeEvents(lkji_event_guids, numTiles);

        countUpdate(descendantCounts, numTiles);
        verifyCount(descendantCounts, numTiles);
    }
    freeMatrix(matrix, input_solution);
    return 0;
}
