#ifndef CORE_JOBS_H
#define CORE_JOBS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef void (*CoreJobFn)(void *user_ctx);

typedef enum CoreJobsOverflowPolicy {
    CORE_JOBS_OVERFLOW_REJECT = 0,
    CORE_JOBS_OVERFLOW_DROP_OLDEST = 1
} CoreJobsOverflowPolicy;

typedef struct CoreJob {
    CoreJobFn fn;
    void *user_ctx;
} CoreJob;

typedef struct CoreJobsStats {
    uint64_t enqueued;
    uint64_t executed;
    uint64_t dropped;
    uint64_t budget_stops;
    uint64_t dropped_oldest;
} CoreJobsStats;

typedef struct CoreJobs {
    CoreJob *slots;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    CoreJobsOverflowPolicy overflow_policy;
    CoreJobsStats stats;
} CoreJobs;

bool core_jobs_init(CoreJobs *jobs, CoreJob *backing, size_t capacity);
bool core_jobs_init_ex(
    CoreJobs *jobs,
    CoreJob *backing,
    size_t capacity,
    CoreJobsOverflowPolicy overflow_policy);
bool core_jobs_enqueue(CoreJobs *jobs, CoreJobFn fn, void *user_ctx);
size_t core_jobs_run_budget(CoreJobs *jobs, uint64_t max_budget_ms);
size_t core_jobs_run_n(CoreJobs *jobs, size_t max_jobs);
size_t core_jobs_pending(const CoreJobs *jobs);
CoreJobsStats core_jobs_stats(const CoreJobs *jobs);

#endif
