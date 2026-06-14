#include "core_time_internal.h"

#include <limits.h>
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

    if (ts.tv_sec < 0) {
        *out_now_ns = 0;
        return false;
    }

    {
        CoreTimeNs sec_ns;
        CoreTimeNs nsec = (CoreTimeNs)ts.tv_nsec;
        if ((CoreTimeNs)ts.tv_sec > (UINT64_MAX / 1000000000ULL)) {
            *out_now_ns = UINT64_MAX;
            return true;
        }
        sec_ns = (CoreTimeNs)ts.tv_sec * 1000000000ULL;
        if (UINT64_MAX - sec_ns < nsec) {
            *out_now_ns = UINT64_MAX;
            return true;
        }
        *out_now_ns = sec_ns + nsec;
    }
    return true;
}
