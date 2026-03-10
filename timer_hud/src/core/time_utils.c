#include "time_utils.h"
#include <time.h>

uint64_t get_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

bool has_interval_elapsed(uint64_t* last_ns, uint64_t interval_ms) {
    uint64_t now = get_time_ns();
    if (*last_ns == 0 || (now - *last_ns) / 1e6 >= interval_ms) {
        *last_ns = now;
        return true;
    }
    return false;
}

