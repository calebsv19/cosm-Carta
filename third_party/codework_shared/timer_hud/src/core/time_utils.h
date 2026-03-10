#ifndef TIME_UTILS_H
#define TIME_UTILS_H

#include <stdint.h>
#include <stdbool.h>

// Monotonic time in nanoseconds
uint64_t get_time_ns(void);

// Returns true if `interval_ms` has elapsed since `*last_ns`
bool has_interval_elapsed(uint64_t* last_ns, uint64_t interval_ms);

#endif

