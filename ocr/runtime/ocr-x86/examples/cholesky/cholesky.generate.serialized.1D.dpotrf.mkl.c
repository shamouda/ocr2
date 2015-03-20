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
#define INDEX_1D(x,y) ( (y) * tileSize + x ) 
#else
assert ( 0 && "MKL works only with column-major")
#define INDEX_1D(x,y) ( (x) * tileSize + y ) 
#endif

struct timeval a,b,c,d;

#define SEED 1000

u8 sequential_cholesky_task ( int tileSize, double* aBlock ) {
    static int status = 0;
    dpotrf("L", &tileSize, aBlock, &tileSize, &status);
}

u8 wrap_up_task (int tileSize, double* matrix, double* input_solution ) {
    int i, j, i_b, j_b;
    double* temp;
    gettimeofday(&b,0);
    printf("The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_stop();
#endif

#ifdef VERIFY
    long double max_diff = -100.0;

    for( i_b = 0; i_b < tileSize; ++i_b) {
        for(j_b = 0; j_b <= i_b; ++j_b) {
            long double diff = (long double) matrix[ INDEX_1D(i_b,j_b) ] - (long double)input_solution[ (i_b*tileSize + j_b)]; 
            if ( diff < 0.0 ) diff = 0 - diff;
            if ( diff > max_diff ) max_diff = diff;
        }
    }

    printf("epsilon: %Lf\n", max_diff);
#endif
}

inline static double* readMatrix( int matrixSize, FILE* in ) {
    int i,j;
    double *A = (double*) malloc (sizeof(double*)*matrixSize*matrixSize);

    for( i = 0; i < matrixSize; ++i ) {
        for( j = 0; j < matrixSize-1; ++j )
            fscanf(in, "%lf ", &A[j*matrixSize+i]);
        fscanf(in, "%lf\n", &A[j*matrixSize+i]);
    }
    return A;
}

inline static double* cloneMatrix( int matrixSize, double* matrix ) {
    int i,j;
    double *A = (double*) malloc (sizeof(double*)*matrixSize*matrixSize);

    for( i = 0; i < matrixSize*matrixSize; ++i ) {
        A[i] = matrix[i];
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

int main( int argc, char* argv[] ) {
    double *matrix, *input_solution;

    int matrixSize = -1;

    if ( argc !=  2 ) {
        printf("Usage: ./cholesky matrixSize (found %d args)\n", argc);
        return 1;
    }

    matrixSize = atoi(argv[1]);


    createMatrix ( &matrix, &input_solution, matrixSize );


#ifdef HPCTOOLKIT
    hpctoolkit_sampling_start();
#endif
    gettimeofday(&a,0);

    sequential_cholesky_task ( matrixSize, matrix );

    wrap_up_task ( matrixSize, matrix, input_solution );
    freeMatrix(matrix, input_solution);
    return 0;
}
