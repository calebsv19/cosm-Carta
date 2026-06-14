#include "core_workers.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct CounterCtx {
    int *counter;
    bool *saw_run;
} CounterCtx;

static void *double_value(void *task_ctx) {
    intptr_t in = (intptr_t)task_ctx;
    return (void *)(in * 2);
}

static void *count_job(void *task_ctx) {
    CounterCtx *ctx = (CounterCtx *)task_ctx;
    if (ctx->saw_run) {
        *ctx->saw_run = true;
    }
    (*ctx->counter)++;
    return NULL;
}

int main(void) {
    void *null_msg = (void *)1;
    assert(!core_queue_mutex_timed_pop(NULL, &null_msg, 1));
    void *completion_backing[16] = {0};
    CoreQueueMutex completions;
    assert(core_queue_mutex_init(&completions, completion_backing, 16));

    pthread_t threads[2] = {0};
    CoreWorkerTask tasks[16] = {0};
    CoreWorkers workers;

    assert(!core_workers_init(NULL, threads, 2, tasks, 16, &completions));
    assert(!core_workers_init(&workers, NULL, 2, tasks, 16, &completions));
    assert(!core_workers_init(&workers, threads, 0, tasks, 16, &completions));
    assert(!core_workers_init(&workers, threads, 2, NULL, 16, &completions));
    assert(!core_workers_init(&workers, threads, 2, tasks, 0, &completions));

    assert(core_workers_init(&workers, threads, 2, tasks, 16, &completions));
    assert(!core_workers_submit(NULL, double_value, (void *)21));
    assert(!core_workers_submit(&workers, NULL, (void *)21));
    assert(core_workers_submit(&workers, double_value, (void *)21));

    void *msg = NULL;
    assert(core_queue_mutex_timed_pop(&completions, &msg, 100));
    assert((intptr_t)msg == 42);

    int counter = 0;
    CounterCtx ctx = {&counter, NULL};
    for (int i = 0; i < 8; ++i) {
        assert(core_workers_submit(&workers, count_job, &ctx));
    }

    CoreWorkersStats stats_before_shutdown = core_workers_stats(&workers);
    assert(stats_before_shutdown.submitted >= 9);
    core_workers_shutdown_with_mode(&workers, CORE_WORKERS_SHUTDOWN_DRAIN);
    assert(counter >= 8);
    assert(!core_workers_submit(&workers, count_job, &ctx));

    CoreWorkers workers2;
    assert(core_workers_init(&workers2, threads, 2, tasks, 16, &completions));
    bool saw_run = false;
    CounterCtx cancel_ctx = {&counter, &saw_run};
    for (int i = 0; i < 8; ++i) {
        assert(core_workers_submit(&workers2, count_job, &cancel_ctx));
    }
    core_workers_shutdown_with_mode(&workers2, CORE_WORKERS_SHUTDOWN_CANCEL);
    CoreWorkersStats canceled_stats = core_workers_stats(&workers2);
    assert(canceled_stats.canceled <= canceled_stats.submitted);
    assert(!core_workers_submit(&workers2, count_job, &cancel_ctx));

    pthread_t small_threads[1] = {0};
    CoreWorkerTask small_tasks[1] = {0};
    CoreWorkers workers3;
    assert(core_workers_init(&workers3, small_threads, 1, small_tasks, 1, NULL));
    assert(core_workers_submit(&workers3, count_job, &ctx));
    assert(!core_workers_submit(&workers3, count_job, &ctx));
    CoreWorkersStats full_stats = core_workers_stats(&workers3);
    assert(full_stats.rejected >= 1);
    core_workers_shutdown(&workers3);

    core_queue_mutex_destroy(&completions);
    return 0;
}
