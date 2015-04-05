#include "stdio.h"
#include "cilk/cilk.h"
#include "sys/time.h"

struct timeval a,b;
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
        case 0: return 0;break;
        case 1: return 1;break;
        default: return inefficientSerialFib(n-2) + inefficientSerialFib(n-1);
                 break;
    };
    return 0ULL;
}

void cilk_fib( int n, int cutoff, u64_t* p_result ) {
    if ( n > cutoff ) {
        u64_t leftResult;
        u64_t rightResult;
        cilk_spawn cilk_fib(n-1, cutoff, &leftResult);
        cilk_fib(n-2, cutoff, &rightResult);
        cilk_sync;
        *p_result=leftResult+rightResult;
    } else {
        *p_result = inefficientSerialFib(n);
    }
}

int main( int argc, char** argv ) {
    int n = -1;
    int cutoff = -1;
    u64_t result;

    n = atoi(argv[1]);
    cutoff = atoi(argv[2]);
    gettimeofday(&a,0);
    cilk_spawn cilk_fib( n, cutoff, &result);
    cilk_sync;
    gettimeofday(&b,0);
    fprintf(stderr,"The computation took %f seconds\r\n",((b.tv_sec - a.tv_sec)*1000000+(b.tv_usec - a.tv_usec))*1.0/1000000);
    fprintf(stderr,"result: %llu\n", result);
    fprintf(stderr,"dynamic result: %llu\n", efficientSerialFib(n));
    return 0;
}


