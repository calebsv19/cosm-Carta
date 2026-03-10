/*
 * core_time.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_time.h"

#include <limits.h>
#include <stddef.h>

#include "core_time_internal.h"

static CoreTimeProvider g_provider = {0};

bool core_time_set_provider(CoreTimeProvider provider) {
    if (!provider.now_fn) {
        return false;
    }
    g_provider = provider;
    return true;
}

void core_time_reset_provider(void) {
    g_provider.now_fn = NULL;
    g_provider.ctx = NULL;
}

CoreTimeNs core_time_now_ns(void) {
    CoreTimeNs now_ns = 0;
    if (g_provider.now_fn) {
        if (g_provider.now_fn(g_provider.ctx, &now_ns)) {
            return now_ns;
        }
        return 0;
    }

    if (!core_time_platform_now_ns(&now_ns)) {
        return 0;
    }
    return now_ns;
}

CoreTimeNs core_time_diff_ns(CoreTimeNs a, CoreTimeNs b) {
    return (a >= b) ? (a - b) : (b - a);
}

CoreTimeNs core_time_add_ns(CoreTimeNs base, CoreTimeNs delta) {
    if (UINT64_MAX - base < delta) {
        return UINT64_MAX;
    }
    return base + delta;
}

int core_time_cmp_ns(CoreTimeNs a, CoreTimeNs b) {
    if (a < b) return -1;
    if (a > b) return 1;
    return 0;
}

double core_time_ns_to_seconds(CoreTimeNs ns) {
    return (double)ns / 1000000000.0;
}

CoreTimeNs core_time_seconds_to_ns(double seconds) {
    if (seconds <= 0.0) return 0;
    if (seconds >= ((double)UINT64_MAX / 1000000000.0)) {
        return UINT64_MAX;
    }
    return (CoreTimeNs)(seconds * 1000000000.0);
}

uint64_t core_time_to_trace_ns(CoreTimeNs ns) {
    return ns;
}

CoreTimeNs core_time_from_trace_ns(uint64_t trace_ns) {
    return trace_ns;
}
