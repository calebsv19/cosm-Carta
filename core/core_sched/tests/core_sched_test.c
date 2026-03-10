#include "core_sched.h"

#include <assert.h>

typedef struct CallbackState {
    int one_shot_count;
    int repeat_count;
    CoreSchedTimerId expected_first;
    CoreSchedTimerId first_seen;
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

int main(void) {
    CoreSchedTimer backing[16] = {0};
    CoreSched sched;
    CallbackState s = {0};

    assert(core_sched_init(&sched, backing, 16));

    CoreSchedTimerId t1 = core_sched_add_timer(&sched, 50, 0, one_shot_cb, &s);
    CoreSchedTimerId t2 = core_sched_add_timer(&sched, 10, 0, one_shot_cb, &s);
    assert(t1 != 0 && t2 != 0);
    s.expected_first = t2;

    assert(core_sched_next_deadline_ns(&sched, 0) == 10);
    assert(core_sched_fire_due(&sched, 9, 10) == 0);
    assert(core_sched_fire_due(&sched, 10, 10) == 1);
    assert(s.first_seen == s.expected_first);

    CoreSchedTimerId t3 = core_sched_add_timer(&sched, 15, 0, one_shot_cb, &s);
    assert(t3 != 0);
    assert(core_sched_cancel_timer(&sched, t3));
    assert(!core_sched_cancel_timer(&sched, t3));

    CoreSchedTimerId rt = core_sched_add_timer(&sched, 20, 10, repeat_cb, &s);
    assert(rt != 0);
    assert(core_sched_fire_due(&sched, 55, 10) >= 1);
    assert(s.repeat_count >= 1);

    return 0;
}
