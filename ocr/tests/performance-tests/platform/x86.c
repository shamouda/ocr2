#include "helper.h"
#include <inttypes.h>

double usec_to_sec (long usec) {
    double res = ((double)usec) / 1000000;
    return res;
}

void get_time(struct timeval * t_time) {
    gettimeofday(t_time, NULL);
}

long elapsed_usec(struct timeval * t_start, struct timeval * t_stop) {
    long start_usec = t_start->tv_sec*1000000+t_start->tv_usec;
    long stop_usec = t_stop->tv_sec*1000000+t_stop->tv_usec;
    long elapsed = stop_usec - start_usec;
    return elapsed;
}

double elapsed_sec(struct timeval * t_start, struct timeval * t_stop) {
    long elapsed = elapsed_usec(t_start, t_stop);
    return (((double)elapsed)/1000000);
}

void print_throughput(char * timer_name, unsigned long long nb_instances, double time_sec) {
    printf("Timer Name        : %s\n", timer_name);
    printf("Workload    (unit): %"PRIu64"\n", nb_instances);
    printf("Duration    (s)   : %f\n", time_sec);
    printf("Throughput  (op/s): %f\n", nb_instances/time_sec);
}

void print_elapsed(double time_sec) {
    printf("Elapsed        (s): %f\n", time_sec);
}

void print_elapsed_usec(long time_usec) {
    printf("Elapsed       (us): %"PRId64"\n", time_usec);
}

void summary_throughput_timer(timestamp_t * start_t, timestamp_t * stop_t, unsigned long long instances) {
    double secs = elapsed_sec(start_t, stop_t);
    summary_throughput_dbl(secs, instances);
}

void summary_throughput_dbl(double secs, unsigned long long instances) {
    print_elapsed(secs);
    print_throughput("Summary", instances, secs);
}

double avg_usec(long * array, int length) {
    int i = 0;
    long acc = 0;
    while (i < length) {
        acc += array[i];
        i++;
    }
    double res = (((double)acc)/length);
    return res;
}
