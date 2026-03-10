#include "core_sched.h"

#include <assert.h>
#include <stdint.h>

#define TIMER_COUNT 1024

static uint32_t lcg(uint32_t *state) {
    *state = (*state * 1664525u) + 1013904223u;
    return *state;
}

static void inc_cb(CoreSchedTimerId id, void *user_ctx) {
    (void)id;
    int *count = (int *)user_ctx;
    (*count)++;
}

int main(void) {
    CoreSchedTimer backing[TIMER_COUNT + 16] = {0};
    CoreSched sched;
    assert(core_sched_init(&sched, backing, TIMER_COUNT + 16));

    int fired = 0;
    uint32_t rng = 12345u;

    CoreSchedTimerId ids[TIMER_COUNT] = {0};
    for (int i = 0; i < TIMER_COUNT; ++i) {
        uint64_t deadline = (uint64_t)(lcg(&rng) % 5000u);
        ids[i] = core_sched_add_timer(&sched, deadline, 0, inc_cb, &fired);
        assert(ids[i] != 0);
    }

    /* cancel some deterministic subset */
    int canceled = 0;
    for (int i = 0; i < TIMER_COUNT; i += 7) {
        if (core_sched_cancel_timer(&sched, ids[i])) {
            canceled++;
        }
    }

    size_t total = core_sched_fire_due(&sched, 5000, TIMER_COUNT + 16);
    assert((int)total == TIMER_COUNT - canceled);
    assert(fired == TIMER_COUNT - canceled);

    return 0;
}
