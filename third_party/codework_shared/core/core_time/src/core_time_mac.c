#include "core_time_internal.h"

#include <mach/mach_time.h>

bool core_time_platform_now_ns(CoreTimeNs *out_now_ns) {
    if (!out_now_ns) {
        return false;
    }

    static mach_timebase_info_data_t info = {0, 0};
    if (info.denom == 0) {
        if (mach_timebase_info(&info) != KERN_SUCCESS || info.denom == 0) {
            *out_now_ns = 0;
            return false;
        }
    }

    uint64_t ticks = mach_absolute_time();
    __uint128_t scaled = ((__uint128_t)ticks * (__uint128_t)info.numer) / (__uint128_t)info.denom;
    if (scaled > UINT64_MAX) {
        *out_now_ns = UINT64_MAX;
    } else {
        *out_now_ns = (uint64_t)scaled;
    }
    return true;
}
