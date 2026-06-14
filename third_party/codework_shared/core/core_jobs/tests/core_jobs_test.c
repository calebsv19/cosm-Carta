#include "core_jobs.h"

#include <assert.h>
#include <limits.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_time.h"

typedef struct FakeClock {
    uint64_t value;
    uint64_t step;
    bool fail;
} FakeClock;

static bool fake_now(void *ctx, uint64_t *out_now_ns) {
    FakeClock *clock = (FakeClock *)ctx;
    if (clock->fail) {
        return false;
    }
    *out_now_ns = clock->value;
    clock->value += clock->step;
    return true;
}

static void bump(void *user_ctx) {
    int *value = (int *)user_ctx;
    (*value)++;
}

typedef struct OrderState {
    int values[8];
    size_t count;
} OrderState;

static void record_order(void *user_ctx) {
    OrderState *state = (OrderState *)user_ctx;
    state->values[state->count] = (int)state->count;
    state->count++;
}

int main(void) {
    CoreJob backing[8] = {0};
    CoreJobs jobs;
    int v = 0;

    assert(!core_jobs_init(NULL, backing, 8));
    assert(!core_jobs_init(&jobs, NULL, 8));
    assert(!core_jobs_init(&jobs, backing, 0));
    assert(!core_jobs_init_ex(&jobs, backing, 8, (CoreJobsOverflowPolicy)99));
    assert(core_jobs_init(&jobs, backing, 8));
    assert(!core_jobs_enqueue(NULL, bump, &v));
    assert(!core_jobs_enqueue(&jobs, NULL, &v));
    assert(core_jobs_enqueue(&jobs, bump, &v));
    assert(core_jobs_enqueue(&jobs, bump, &v));

    FakeClock clock = {0, 1000000, false};
    CoreTimeProvider p = {fake_now, &clock};
    assert(core_time_set_provider(p));

    size_t ran = core_jobs_run_budget(&jobs, 1);
    assert(ran == 1);
    assert(v == 1);
    assert(core_jobs_pending(&jobs) == 1);

    ran = core_jobs_run_budget(&jobs, 0);
    assert(ran == 1);
    assert(v == 2);

    assert(core_jobs_run_budget(NULL, 1) == 0);
    assert(core_jobs_run_n(NULL, 1) == 0);
    assert(core_jobs_run_n(&jobs, 0) == 0);
    assert(core_jobs_pending(NULL) == 0);

    core_time_reset_provider();

    CoreJobsStats stats = core_jobs_stats(&jobs);
    assert(stats.enqueued == 2);
    assert(stats.executed == 2);
    assert(stats.budget_stops >= 1);

    CoreJobsStats empty_stats = core_jobs_stats(NULL);
    assert(empty_stats.enqueued == 0);
    assert(empty_stats.executed == 0);
    assert(empty_stats.dropped == 0);
    assert(empty_stats.budget_stops == 0);
    assert(empty_stats.dropped_oldest == 0);

    CoreJobs jobs_drop;
    CoreJob drop_backing[2] = {0};
    assert(core_jobs_init_ex(&jobs_drop, drop_backing, 2, CORE_JOBS_OVERFLOW_DROP_OLDEST));
    v = 0;
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_enqueue(&jobs_drop, bump, &v));
    assert(core_jobs_run_n(&jobs_drop, 2) == 2);
    assert(v == 2);
    stats = core_jobs_stats(&jobs_drop);
    assert(stats.enqueued == 3);
    assert(stats.executed == 2);
    assert(stats.dropped == 0);
    assert(stats.dropped_oldest == 1);

    CoreJobs jobs_reject;
    CoreJob reject_backing[2] = {0};
    assert(core_jobs_init(&jobs_reject, reject_backing, 2));
    assert(core_jobs_enqueue(&jobs_reject, bump, &v));
    assert(core_jobs_enqueue(&jobs_reject, bump, &v));
    assert(!core_jobs_enqueue(&jobs_reject, bump, &v));
    stats = core_jobs_stats(&jobs_reject);
    assert(stats.dropped == 1);
    assert(stats.dropped_oldest == 0);
    assert(core_jobs_pending(&jobs_reject) == 2);

    CoreJobs wrap_jobs;
    CoreJob wrap_backing[4] = {0};
    OrderState order = {0};
    assert(core_jobs_init(&wrap_jobs, wrap_backing, 4));
    assert(core_jobs_enqueue(&wrap_jobs, record_order, &order));
    assert(core_jobs_enqueue(&wrap_jobs, record_order, &order));
    assert(core_jobs_run_n(&wrap_jobs, 2) == 2);
    assert(core_jobs_pending(&wrap_jobs) == 0);
    assert(core_jobs_enqueue(&wrap_jobs, record_order, &order));
    assert(core_jobs_enqueue(&wrap_jobs, record_order, &order));
    assert(core_jobs_enqueue(&wrap_jobs, record_order, &order));
    assert(core_jobs_run_n(&wrap_jobs, 3) == 3);
    assert(order.count == 5);
    for (size_t i = 0; i < order.count; ++i) {
        assert(order.values[i] == (int)i);
    }

    CoreJobs budget_jobs;
    CoreJob budget_backing[3] = {0};
    assert(core_jobs_init(&budget_jobs, budget_backing, 3));
    v = 0;
    assert(core_jobs_enqueue(&budget_jobs, bump, &v));
    assert(core_jobs_enqueue(&budget_jobs, bump, &v));
    clock = (FakeClock){0, 1000000, false};
    p = (CoreTimeProvider){fake_now, &clock};
    assert(core_time_set_provider(p));
    assert(core_jobs_run_budget(&budget_jobs, 2) == 2);
    assert(v == 2);
    core_time_reset_provider();

    CoreJobs huge_budget_jobs;
    CoreJob huge_budget_backing[2] = {0};
    assert(core_jobs_init(&huge_budget_jobs, huge_budget_backing, 2));
    v = 0;
    assert(core_jobs_enqueue(&huge_budget_jobs, bump, &v));
    assert(core_jobs_enqueue(&huge_budget_jobs, bump, &v));
    clock = (FakeClock){0, UINT64_MAX / 2u, false};
    p = (CoreTimeProvider){fake_now, &clock};
    assert(core_time_set_provider(p));
    assert(core_jobs_run_budget(&huge_budget_jobs, UINT64_MAX) == 2);
    assert(v == 2);
    core_time_reset_provider();

    return 0;
}
