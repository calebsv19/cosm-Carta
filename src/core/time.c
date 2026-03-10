#include "core/time.h"

#include "core_time.h"

// Returns current time in seconds from a monotonic clock.
double time_now_seconds(void) {
    return core_time_ns_to_seconds(core_time_now_ns());
}
