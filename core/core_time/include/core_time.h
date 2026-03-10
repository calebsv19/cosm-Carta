#ifndef CORE_TIME_H
#define CORE_TIME_H

#include <stdbool.h>
#include <stdint.h>

typedef uint64_t CoreTimeNs;

typedef bool (*CoreTimeNowFn)(void *ctx, CoreTimeNs *out_now_ns);

typedef struct CoreTimeProvider {
    CoreTimeNowFn now_fn;
    void *ctx;
} CoreTimeProvider;

bool core_time_set_provider(CoreTimeProvider provider);
void core_time_reset_provider(void);

CoreTimeNs core_time_now_ns(void);
CoreTimeNs core_time_diff_ns(CoreTimeNs a, CoreTimeNs b);
CoreTimeNs core_time_add_ns(CoreTimeNs base, CoreTimeNs delta);
int core_time_cmp_ns(CoreTimeNs a, CoreTimeNs b);

double core_time_ns_to_seconds(CoreTimeNs ns);
CoreTimeNs core_time_seconds_to_ns(double seconds);

/* Trace timestamp interop helpers (v1: nanosecond identity mapping). */
uint64_t core_time_to_trace_ns(CoreTimeNs ns);
CoreTimeNs core_time_from_trace_ns(uint64_t trace_ns);

#endif
