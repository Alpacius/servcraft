#include    "./p7r_timing.h"


uint64_t get_timestamp_ms_current(void) {
    struct timespec timeval;
    clock_gettime(CLOCK_REALTIME_COARSE, &timeval);
    return ((uint64_t) timeval.tv_sec * 1000) + ((uint64_t) timeval.tv_nsec / (1000 * 1000));
}

uint64_t get_timestamp_ms_by_diff(uint64_t diff) {
    return get_timestamp_ms_current() + diff;
}
