#ifndef SYS_SHIMS_SHIM_TIME_H
#define SYS_SHIMS_SHIM_TIME_H

#if defined(__clang__) || defined(__GNUC__)
#include_next <stdbool.h>
#include_next <stddef.h>
#include_next <time.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <time.h>
#endif

#include "sys_shims/shim_stddef.h"

static inline bool shim_time_now(time_t *out_now) {
    time_t t;
    if (!out_now) return false;
    t = time(NULL);
    if (t == (time_t)-1) return false;
    *out_now = t;
    return true;
}

static inline bool shim_localtime_safe(const time_t *src, struct tm *out_tm) {
    if (!src || !out_tm) return false;
#if defined(_POSIX_VERSION)
    return localtime_r(src, out_tm) != NULL;
#else
    {
        struct tm *tmp = localtime(src);
        if (!tmp) return false;
        *out_tm = *tmp;
        return true;
    }
#endif
}

static inline bool shim_time_format_utc(const struct tm *tm_value,
                                        char *out,
                                        shim_size_t out_size) {
    if (!tm_value || !out || out_size == 0) return false;
    return strftime(out, out_size, "%Y-%m-%dT%H:%M:%SZ", tm_value) > 0;
}

#endif
