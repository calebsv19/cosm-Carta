#ifndef CORE_SCHED_H
#define CORE_SCHED_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef uint64_t CoreSchedTimerId;
typedef void (*CoreSchedTimerFn)(CoreSchedTimerId id, void *user_ctx);

typedef struct CoreSchedTimer {
    CoreSchedTimerId id;
    uint64_t deadline_ns;
    uint64_t interval_ns;
    CoreSchedTimerFn callback;
    void *user_ctx;
} CoreSchedTimer;

typedef struct CoreSched {
    CoreSchedTimer *heap;
    size_t capacity;
    size_t count;
    CoreSchedTimerId next_id;
} CoreSched;

bool core_sched_init(CoreSched *sched, CoreSchedTimer *backing, size_t capacity);
CoreSchedTimerId core_sched_add_timer(
    CoreSched *sched,
    uint64_t deadline_ns,
    uint64_t interval_ns,
    CoreSchedTimerFn callback,
    void *user_ctx);
bool core_sched_cancel_timer(CoreSched *sched, CoreSchedTimerId id);
uint64_t core_sched_next_deadline_ns(const CoreSched *sched, uint64_t now_ns);
size_t core_sched_fire_due(CoreSched *sched, uint64_t now_ns, size_t max_fires);

#endif
