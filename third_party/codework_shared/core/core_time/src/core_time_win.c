#include "core_time_internal.h"

#if defined(_WIN32)
#include <windows.h>

bool core_time_platform_now_ns(CoreTimeNs *out_now_ns) {
    if (!out_now_ns) return false;

    static LARGE_INTEGER freq = {0};
    LARGE_INTEGER counter;

    if (freq.QuadPart == 0) {
        if (!QueryPerformanceFrequency(&freq) || freq.QuadPart == 0) {
            *out_now_ns = 0;
            return false;
        }
    }

    if (!QueryPerformanceCounter(&counter)) {
        *out_now_ns = 0;
        return false;
    }

    long double ns = ((long double)counter.QuadPart * 1000000000.0L) / (long double)freq.QuadPart;
    if (ns < 0.0L) {
        *out_now_ns = 0;
    } else if (ns > (long double)UINT64_MAX) {
        *out_now_ns = UINT64_MAX;
    } else {
        *out_now_ns = (uint64_t)ns;
    }
    return true;
}
#endif
