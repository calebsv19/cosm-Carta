#include "core_workers.h"

#include <assert.h>
#include <stdint.h>

typedef struct CounterCtx {
    int *counter;
} CounterCtx;

static void *double_value(void *task_ctx) {
    intptr_t in = (intptr_t)task_ctx;
    return (void *)(in * 2);
}

static void *count_job(void *task_ctx) {
    CounterCtx *ctx = (CounterCtx *)task_ctx;
    (*ctx->counter)++;
    return NULL;
}

int main(void) {
    void *completion_backing[16] = {0};
    CoreQueueMutex completions;
    assert(core_queue_mutex_init(&completions, completion_backing, 16));

    pthread_t threads[2] = {0};
    CoreWorkerTask tasks[16] = {0};
    CoreWorkers workers;

    assert(core_workers_init(&workers, threads, 2, tasks, 16, &completions));
    assert(core_workers_submit(&workers, double_value, (void *)21));

    void *msg = NULL;
    assert(core_queue_mutex_timed_pop(&completions, &msg, 100));
    assert((intptr_t)msg == 42);

    int counter = 0;
    CounterCtx ctx = {&counter};
    for (int i = 0; i < 8; ++i) {
        assert(core_workers_submit(&workers, count_job, &ctx));
    }

    CoreWorkersStats stats_before_shutdown = core_workers_stats(&workers);
    assert(stats_before_shutdown.submitted >= 9);
    core_workers_shutdown_with_mode(&workers, CORE_WORKERS_SHUTDOWN_DRAIN);
    assert(counter >= 8);

    CoreWorkers workers2;
    assert(core_workers_init(&workers2, threads, 2, tasks, 16, &completions));
    for (int i = 0; i < 8; ++i) {
        assert(core_workers_submit(&workers2, count_job, &ctx));
    }
    core_workers_shutdown_with_mode(&workers2, CORE_WORKERS_SHUTDOWN_CANCEL);

    core_queue_mutex_destroy(&completions);
    return 0;
}
