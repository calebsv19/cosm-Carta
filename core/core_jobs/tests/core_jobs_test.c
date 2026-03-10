#include "core_jobs.h"

#include <assert.h>

#include "core_time.h"

typedef struct FakeClock {
    uint64_t value;
    uint64_t step;
} FakeClock;

static bool fake_now(void *ctx, uint64_t *out_now_ns) {
    FakeClock *clock = (FakeClock *)ctx;
    *out_now_ns = clock->value;
    clock->value += clock->step;
    return true;
}

static void bump(void *user_ctx) {
    int *value = (int *)user_ctx;
    (*value)++;
}

int main(void) {
    CoreJob backing[8] = {0};
    CoreJobs jobs;
    int v = 0;

    assert(core_jobs_init(&jobs, backing, 8));
    assert(core_jobs_enqueue(&jobs, bump, &v));
    assert(core_jobs_enqueue(&jobs, bump, &v));

    FakeClock clock = {0, 1000000};
    CoreTimeProvider p = {fake_now, &clock};
    assert(core_time_set_provider(p));

    size_t ran = core_jobs_run_budget(&jobs, 1);
    assert(ran == 1);
    assert(v == 1);
    assert(core_jobs_pending(&jobs) == 1);

    ran = core_jobs_run_budget(&jobs, 0);
    assert(ran == 1);
    assert(v == 2);

    core_time_reset_provider();

    CoreJobsStats stats = core_jobs_stats(&jobs);
    assert(stats.enqueued == 2);
    assert(stats.executed == 2);
    assert(stats.budget_stops >= 1);

    CoreJobs jobs_drop;
    assert(core_jobs_init_ex(&jobs_drop, backing, 2, CORE_JOBS_OVERFLOW_DROP_OLDEST));
    v = 0;
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_run_n(&jobs_drop, 2) == 2);
    assert(v == 2);
    stats = core_jobs_stats(&jobs_drop);
    assert(stats.dropped_oldest == 1);

    return 0;
}
