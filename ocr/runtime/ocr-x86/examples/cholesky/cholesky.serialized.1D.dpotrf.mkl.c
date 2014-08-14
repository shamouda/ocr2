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

struct timeval a,b;

u8 sequential_cholesky_task ( int tileSize, double* aBlock ) {
    static int status = 0;
    dpotrf("L", &tileSize, aBlock, &tileSize, &status);
}

u8 wrap_up_task (int tileSize, double* matrix ) {
    int i, j, i_b, j_b;
    double* temp;
    gettimeofday(&b,0);
    printf("The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_stop();
#endif

#ifdef VERIFY
    FILE* out = fopen("cholesky.out", "w");

    for( i_b = 0; i_b < tileSize; ++i_b) {
        for(j_b = 0; j_b <= i_b; ++j_b) {
            fprintf( out, "%lf ", matrix[ INDEX_1D(i_b,j_b) ]);
        }
    }
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

inline static freeMatrix ( double* A ) {
    free(A);
}

int main( int argc, char* argv[] ) {
    double *matrix, *tmp;
    FILE *in;

    int matrixSize = -1;

    if ( argc !=  3 ) {
        printf("Usage: ./cholesky matrixSize fileName (found %d args)\n", argc);
        return 1;
    }

    matrixSize = atoi(argv[1]);

    in = fopen(argv[2], "r");
    if( !in ) {
        printf("Cannot find file: %s\n", argv[2]);
        return 1;
    }

    matrix = readMatrix( matrixSize, in );

    fclose(in);

    tmp = cloneMatrix(matrixSize, matrix);
#ifdef HPCTOOLKIT
    hpctoolkit_sampling_start();
#endif
    gettimeofday(&a,0);

    sequential_cholesky_task ( matrixSize, tmp );

    wrap_up_task ( matrixSize, tmp );
    freeMatrix(tmp);
    freeMatrix(matrix);
    return 0;
}
