#ifndef __BENCH_HELPER_H_
#define __BENCH_HELPER_H_

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <sys/time.h>

#define timestamp_t struct timeval

void get_time(struct timeval * t_time);

double elapsed_sec(struct timeval * start, struct timeval * stop);

long elapsed_usec(struct timeval * start, struct timeval * stop);

double avg_usec(long * array, int length);


/* Conversion functions */

double usec_to_sec (long usec);

/* Printing functions */

void print_throughput(char * timer_name, unsigned long long nb_instances, double time_sec);

void summary_throughput_timer(timestamp_t * start_t, timestamp_t * stop_t, unsigned long long instances);

void summary_throughput_dbl(double secs, unsigned long long instances);

#endif
