/*
 * core_sched.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_sched.h"

static bool timer_less(const CoreSchedTimer *a, const CoreSchedTimer *b) {
    if (a->deadline_ns != b->deadline_ns) {
        return a->deadline_ns < b->deadline_ns;
    }
    return a->id < b->id;
}

static void swap_timer(CoreSchedTimer *a, CoreSchedTimer *b) {
    CoreSchedTimer tmp = *a;
    *a = *b;
    *b = tmp;
}

static void sift_up(CoreSched *sched, size_t idx) {
    while (idx > 0) {
        size_t parent = (idx - 1u) / 2u;
        if (!timer_less(&sched->heap[idx], &sched->heap[parent])) {
            break;
        }
        swap_timer(&sched->heap[idx], &sched->heap[parent]);
        idx = parent;
    }
}

static void sift_down(CoreSched *sched, size_t idx) {
    while (true) {
        size_t left = idx * 2u + 1u;
        size_t right = left + 1u;
        size_t smallest = idx;

        if (left < sched->count && timer_less(&sched->heap[left], &sched->heap[smallest])) {
            smallest = left;
        }
        if (right < sched->count && timer_less(&sched->heap[right], &sched->heap[smallest])) {
            smallest = right;
        }

        if (smallest == idx) {
            break;
        }

        swap_timer(&sched->heap[idx], &sched->heap[smallest]);
        idx = smallest;
    }
}

static void heap_push(CoreSched *sched, CoreSchedTimer t) {
    sched->heap[sched->count] = t;
    sift_up(sched, sched->count);
    sched->count++;
}

static CoreSchedTimer heap_pop_min(CoreSched *sched) {
    CoreSchedTimer top = sched->heap[0];
    sched->count--;
    if (sched->count > 0) {
        sched->heap[0] = sched->heap[sched->count];
        sift_down(sched, 0);
    }
    return top;
}

static bool heap_remove_by_id(CoreSched *sched, CoreSchedTimerId id) {
    for (size_t i = 0; i < sched->count; ++i) {
        if (sched->heap[i].id == id) {
            sched->count--;
            if (i != sched->count) {
                sched->heap[i] = sched->heap[sched->count];
                sift_down(sched, i);
                sift_up(sched, i);
            }
            return true;
        }
    }
    return false;
}

bool core_sched_init(CoreSched *sched, CoreSchedTimer *backing, size_t capacity) {
    if (!sched || !backing || capacity == 0) return false;
    sched->heap = backing;
    sched->capacity = capacity;
    sched->count = 0;
    sched->next_id = 1;
    return true;
}

CoreSchedTimerId core_sched_add_timer(
    CoreSched *sched,
    uint64_t deadline_ns,
    uint64_t interval_ns,
    CoreSchedTimerFn callback,
    void *user_ctx) {
    if (!sched || !callback || sched->count >= sched->capacity) {
        return 0;
    }

    CoreSchedTimer t;
    t.id = sched->next_id++;
    t.deadline_ns = deadline_ns;
    t.interval_ns = interval_ns;
    t.callback = callback;
    t.user_ctx = user_ctx;

    heap_push(sched, t);
    return t.id;
}

bool core_sched_cancel_timer(CoreSched *sched, CoreSchedTimerId id) {
    if (!sched || id == 0) return false;
    return heap_remove_by_id(sched, id);
}

uint64_t core_sched_next_deadline_ns(const CoreSched *sched, uint64_t now_ns) {
    if (!sched || sched->count == 0) return 0;
    uint64_t next = sched->heap[0].deadline_ns;
    return (next < now_ns) ? now_ns : next;
}

size_t core_sched_fire_due(CoreSched *sched, uint64_t now_ns, size_t max_fires) {
    if (!sched || max_fires == 0) return 0;

    size_t fired = 0;
    while (sched->count > 0 && fired < max_fires) {
        CoreSchedTimer top = sched->heap[0];
        if (top.deadline_ns > now_ns) {
            break;
        }

        top = heap_pop_min(sched);
        top.callback(top.id, top.user_ctx);
        fired++;

        if (top.interval_ns > 0) {
            uint64_t next_deadline = top.deadline_ns;
            do {
                next_deadline += top.interval_ns;
            } while (next_deadline <= now_ns);
            top.deadline_ns = next_deadline;
            heap_push(sched, top);
        }
    }

    return fired;
}
