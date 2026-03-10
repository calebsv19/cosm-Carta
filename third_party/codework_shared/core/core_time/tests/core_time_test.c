#include "core_time.h"

#include <assert.h>
#include <limits.h>

typedef struct FakeClock {
    CoreTimeNs value;
    CoreTimeNs step;
} FakeClock;

static bool fake_now(void *ctx, CoreTimeNs *out_now_ns) {
    FakeClock *clock = (FakeClock *)ctx;
    *out_now_ns = clock->value;
    clock->value += clock->step;
    return true;
}

int main(void) {
    CoreTimeNs sys_a = core_time_now_ns();
    CoreTimeNs sys_b = core_time_now_ns();
    assert(sys_b >= sys_a);

    FakeClock clock = {1000, 100};
    CoreTimeProvider p = {fake_now, &clock};
    assert(core_time_set_provider(p));

    assert(core_time_now_ns() == 1000);
    assert(core_time_now_ns() == 1100);

    core_time_reset_provider();

    assert(core_time_cmp_ns(10, 10) == 0);
    assert(core_time_cmp_ns(9, 10) < 0);
    assert(core_time_diff_ns(100, 40) == 60);
    assert(core_time_add_ns(10, 5) == 15);
    assert(core_time_add_ns(UINT64_MAX, 1) == UINT64_MAX);

    assert(core_time_seconds_to_ns(0.001) == 1000000ULL);
    assert(core_time_seconds_to_ns(-1.0) == 0);
    assert(core_time_ns_to_seconds(1000000000ULL) == 1.0);

    assert(core_time_to_trace_ns(12345) == 12345);
    assert(core_time_from_trace_ns(67890) == 67890);

    return 0;
}
