#include "core_sched.h"

#include <assert.h>
#include <limits.h>

typedef struct CallbackState {
    int one_shot_count;
    int repeat_count;
    int add_count;
    int cancel_count;
    CoreSchedTimerId expected_first;
    CoreSchedTimerId first_seen;
    CoreSched *sched;
    uint64_t add_deadline_ns;
    CoreSchedTimerId cancel_target;
} CallbackState;

static void one_shot_cb(CoreSchedTimerId id, void *user_ctx) {
    CallbackState *s = (CallbackState *)user_ctx;
    s->one_shot_count++;
    if (s->first_seen == 0) {
        s->first_seen = id;
    }
}

static void repeat_cb(CoreSchedTimerId id, void *user_ctx) {
    (void)id;
    CallbackState *s = (CallbackState *)user_ctx;
    s->repeat_count++;
}

static void add_timer_cb(CoreSchedTimerId id, void *user_ctx) {
    (void)id;
    CallbackState *s = (CallbackState *)user_ctx;
    s->add_count++;
    CoreSchedTimerId added =
        core_sched_add_timer(s->sched, s->add_deadline_ns, 0, one_shot_cb, s);
    assert(added != 0);
}

static void cancel_timer_cb(CoreSchedTimerId id, void *user_ctx) {
    (void)id;
    CallbackState *s = (CallbackState *)user_ctx;
    s->cancel_count++;
    assert(core_sched_cancel_timer(s->sched, s->cancel_target));
}

int main(void) {
    CoreSchedTimer backing[16] = {0};
    CoreSched sched;
    CallbackState s = {0};

    assert(!core_sched_init(NULL, backing, 16));
    assert(!core_sched_init(&sched, NULL, 16));
    assert(!core_sched_init(&sched, backing, 0));
    assert(core_sched_init(&sched, backing, 16));
    s.sched = &sched;

    assert(core_sched_add_timer(NULL, 1, 0, one_shot_cb, &s) == 0);
    assert(core_sched_add_timer(&sched, 1, 0, NULL, &s) == 0);
    CoreSchedTimerId t1 = core_sched_add_timer(&sched, 50, 0, one_shot_cb, &s);
    CoreSchedTimerId t2 = core_sched_add_timer(&sched, 10, 0, one_shot_cb, &s);
    assert(t1 != 0 && t2 != 0);
    s.expected_first = t2;

    assert(core_sched_next_deadline_ns(NULL, 0) == 0);
    assert(core_sched_next_deadline_ns(&sched, 0) == 10);
    assert(core_sched_next_deadline_ns(&sched, 25) == 25);
    assert(core_sched_fire_due(&sched, 9, 10) == 0);
    assert(core_sched_fire_due(NULL, 10, 10) == 0);
    assert(core_sched_fire_due(&sched, 10, 0) == 0);
    assert(core_sched_fire_due(&sched, 10, 10) == 1);
    assert(s.first_seen == s.expected_first);

    CoreSchedTimerId t3 = core_sched_add_timer(&sched, 15, 0, one_shot_cb, &s);
    assert(t3 != 0);
    assert(core_sched_cancel_timer(&sched, t3));
    assert(!core_sched_cancel_timer(&sched, t3));
    assert(!core_sched_cancel_timer(NULL, t3));
    assert(!core_sched_cancel_timer(&sched, 0));

    CoreSchedTimerId rt = core_sched_add_timer(&sched, 20, 10, repeat_cb, &s);
    assert(rt != 0);
    assert(core_sched_fire_due(&sched, 55, 1) == 1);
    assert(s.repeat_count == 1);
    assert(core_sched_next_deadline_ns(&sched, 55) == 55);
    assert(core_sched_fire_due(&sched, 55, 10) == 1);
    assert(s.repeat_count >= 1);

    CoreSchedTimer repeat_backing[2] = {0};
    CoreSched repeat_sched;
    CallbackState repeat_state = {0};
    assert(core_sched_init(&repeat_sched, repeat_backing, 2));
    assert(core_sched_add_timer(&repeat_sched, 20, 10, repeat_cb, &repeat_state) != 0);
    assert(core_sched_fire_due(&repeat_sched, 55, 1) == 1);
    assert(repeat_state.repeat_count == 1);
    assert(core_sched_next_deadline_ns(&repeat_sched, 55) == 60);

    CoreSchedTimer ordering_backing[4] = {0};
    CoreSched ordering_sched;
    CallbackState ordering = {0};
    assert(core_sched_init(&ordering_sched, ordering_backing, 4));
    ordering.sched = &ordering_sched;
    CoreSchedTimerId oa = core_sched_add_timer(&ordering_sched, 100, 0, one_shot_cb, &ordering);
    CoreSchedTimerId ob = core_sched_add_timer(&ordering_sched, 100, 0, one_shot_cb, &ordering);
    assert(oa != 0 && ob != 0);
    ordering.expected_first = oa;
    assert(core_sched_fire_due(&ordering_sched, 100, 2) == 2);
    assert(ordering.first_seen == ordering.expected_first);

    CoreSchedTimer full_backing[2] = {0};
    CoreSched full_sched;
    assert(core_sched_init(&full_sched, full_backing, 2));
    assert(core_sched_add_timer(&full_sched, 1, 0, one_shot_cb, &s) != 0);
    assert(core_sched_add_timer(&full_sched, 2, 0, one_shot_cb, &s) != 0);
    assert(core_sched_add_timer(&full_sched, 3, 0, one_shot_cb, &s) == 0);

    CoreSchedTimer callback_backing[8] = {0};
    CoreSched callback_sched;
    CallbackState callback_state = {0};
    assert(core_sched_init(&callback_sched, callback_backing, 8));
    callback_state.sched = &callback_sched;
    callback_state.add_deadline_ns = 5;
    callback_state.cancel_target =
        core_sched_add_timer(&callback_sched, 7, 0, one_shot_cb, &callback_state);
    assert(callback_state.cancel_target != 0);
    assert(core_sched_add_timer(&callback_sched, 5, 0, add_timer_cb, &callback_state) != 0);
    assert(core_sched_add_timer(&callback_sched, 5, 0, cancel_timer_cb, &callback_state) != 0);
    assert(core_sched_fire_due(&callback_sched, 5, 8) == 3);
    assert(callback_state.add_count == 1);
    assert(callback_state.cancel_count == 1);
    assert(!core_sched_cancel_timer(&callback_sched, callback_state.cancel_target));

    CoreSchedTimer overflow_backing[2] = {0};
    CoreSched overflow_sched;
    CallbackState overflow_state = {0};
    assert(core_sched_init(&overflow_sched, overflow_backing, 2));
    assert(core_sched_add_timer(&overflow_sched, UINT64_MAX - 2u, 5u, repeat_cb, &overflow_state) != 0);
    assert(core_sched_fire_due(&overflow_sched, UINT64_MAX - 1u, 4) == 1);
    assert(overflow_state.repeat_count == 1);
    assert(core_sched_next_deadline_ns(&overflow_sched, UINT64_MAX - 1u) == 0);

    CoreSchedTimer wrap_backing[2] = {0};
    CoreSched wrap_sched;
    assert(core_sched_init(&wrap_sched, wrap_backing, 2));
    wrap_sched.next_id = UINT64_MAX;
    assert(core_sched_add_timer(&wrap_sched, 1, 0, one_shot_cb, &s) == UINT64_MAX);
    assert(core_sched_add_timer(&wrap_sched, 2, 0, one_shot_cb, &s) == 0);

    return 0;
}
