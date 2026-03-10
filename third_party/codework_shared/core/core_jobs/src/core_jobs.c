/*
 * core_jobs.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_jobs.h"

#include "core_time.h"

bool core_jobs_init(CoreJobs *jobs, CoreJob *backing, size_t capacity) {
    return core_jobs_init_ex(jobs, backing, capacity, CORE_JOBS_OVERFLOW_REJECT);
}

bool core_jobs_init_ex(
    CoreJobs *jobs,
    CoreJob *backing,
    size_t capacity,
    CoreJobsOverflowPolicy overflow_policy) {
    if (!jobs || !backing || capacity == 0) return false;
    jobs->slots = backing;
    jobs->capacity = capacity;
    jobs->head = 0;
    jobs->tail = 0;
    jobs->count = 0;
    jobs->overflow_policy = overflow_policy;
    jobs->stats = (CoreJobsStats){0};
    return true;
}

bool core_jobs_enqueue(CoreJobs *jobs, CoreJobFn fn, void *user_ctx) {
    if (!jobs || !fn) return false;
    if (jobs->count == jobs->capacity) {
        if (jobs->overflow_policy == CORE_JOBS_OVERFLOW_DROP_OLDEST) {
            jobs->head = (jobs->head + 1u) % jobs->capacity;
            jobs->count--;
            jobs->stats.dropped_oldest++;
        } else {
            jobs->stats.dropped++;
            return false;
        }
    }

    jobs->slots[jobs->tail].fn = fn;
    jobs->slots[jobs->tail].user_ctx = user_ctx;
    jobs->tail = (jobs->tail + 1u) % jobs->capacity;
    jobs->count++;
    jobs->stats.enqueued++;
    return true;
}

size_t core_jobs_run_budget(CoreJobs *jobs, uint64_t max_budget_ms) {
    if (!jobs) return 0;

    const bool unlimited = (max_budget_ms == 0);
    const uint64_t budget_ns = max_budget_ms * 1000000ULL;
    const uint64_t start_ns = core_time_now_ns();

    size_t ran = 0;
    while (jobs->count > 0) {
        if (!unlimited) {
            uint64_t elapsed = core_time_diff_ns(core_time_now_ns(), start_ns);
            if (elapsed > budget_ns) {
                jobs->stats.budget_stops++;
                break;
            }
        }

        CoreJob job = jobs->slots[jobs->head];
        jobs->head = (jobs->head + 1u) % jobs->capacity;
        jobs->count--;

        job.fn(job.user_ctx);
        ran++;
        jobs->stats.executed++;
    }

    return ran;
}

size_t core_jobs_run_n(CoreJobs *jobs, size_t max_jobs) {
    if (!jobs || max_jobs == 0) return 0;

    size_t ran = 0;
    while (jobs->count > 0 && ran < max_jobs) {
        CoreJob job = jobs->slots[jobs->head];
        jobs->head = (jobs->head + 1u) % jobs->capacity;
        jobs->count--;
        job.fn(job.user_ctx);
        ran++;
        jobs->stats.executed++;
    }
    return ran;
}

size_t core_jobs_pending(const CoreJobs *jobs) {
    if (!jobs) return 0;
    return jobs->count;
}

CoreJobsStats core_jobs_stats(const CoreJobs *jobs) {
    CoreJobsStats empty = {0};
    if (!jobs) return empty;
    return jobs->stats;
}
