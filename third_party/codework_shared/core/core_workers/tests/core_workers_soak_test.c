#include "core_workers.h"

#include <assert.h>
#include <stdint.h>

enum { TASK_COUNT = 6000 };

typedef struct TaskCtx {
    int value;
} TaskCtx;

static void *task_fn(void *task_ctx) {
    TaskCtx *ctx = (TaskCtx *)task_ctx;
    return (void *)(intptr_t)(ctx->value + 1);
}

int main(void) {
    void *completion_backing[8192] = {0};
    CoreQueueMutex completions;
    assert(core_queue_mutex_init(&completions, completion_backing, 8192));

    pthread_t threads[4] = {0};
    CoreWorkerTask task_slots[4096] = {0};
    TaskCtx contexts[TASK_COUNT];
    for (int i = 0; i < TASK_COUNT; ++i) {
        contexts[i].value = i;
    }

    CoreWorkers workers;
    assert(core_workers_init(&workers, threads, 4, task_slots, 4096, &completions));

    int submitted = 0;
    for (int i = 0; i < TASK_COUNT; ++i) {
        if (core_workers_submit(&workers, task_fn, &contexts[i])) {
            submitted++;
        }
    }

    int completions_seen = 0;
    while (completions_seen < submitted) {
        void *msg = NULL;
        if (core_queue_mutex_timed_pop(&completions, &msg, 10)) {
            assert((intptr_t)msg > 0);
            completions_seen++;
        }
    }

    core_workers_shutdown(&workers);

    CoreWorkersStats stats = core_workers_stats(&workers);
    assert((int)stats.submitted == submitted);
    assert((int)stats.completed == submitted);

    core_queue_mutex_destroy(&completions);
    return 0;
}
