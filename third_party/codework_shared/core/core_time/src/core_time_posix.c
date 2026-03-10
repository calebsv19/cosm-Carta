#include "core_time_internal.h"

#include <time.h>

bool core_time_platform_now_ns(CoreTimeNs *out_now_ns) {
    if (!out_now_ns) {
        return false;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        *out_now_ns = 0;
        return false;
    }

    *out_now_ns = ((CoreTimeNs)ts.tv_sec * 1000000000ULL) + (CoreTimeNs)ts.tv_nsec;
    return true;
}
