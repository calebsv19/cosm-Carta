#ifndef CORE_WORKERS_H
#define CORE_WORKERS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pthread.h>

#include "core_queue.h"

typedef void *(*CoreWorkerTaskFn)(void *task_ctx);

typedef struct CoreWorkerTask {
    CoreWorkerTaskFn fn;
    void *task_ctx;
} CoreWorkerTask;

typedef enum CoreWorkersShutdownMode {
    CORE_WORKERS_SHUTDOWN_DRAIN = 0,
    CORE_WORKERS_SHUTDOWN_CANCEL = 1
} CoreWorkersShutdownMode;

typedef struct CoreWorkersStats {
    uint64_t submitted;
    uint64_t completed;
    uint64_t rejected;
    uint64_t canceled;
} CoreWorkersStats;

typedef struct CoreWorkers {
    pthread_t *threads;
    size_t thread_count;

    CoreWorkerTask *task_slots;
    size_t task_capacity;
    size_t task_head;
    size_t task_tail;
    size_t task_count;

    bool stop;
    bool cancel_pending;
    pthread_mutex_t mu;
    pthread_cond_t cv;

    CoreQueueMutex *completion_queue;
    CoreWorkersStats stats;
    bool initialized;
} CoreWorkers;

bool core_workers_init(
    CoreWorkers *workers,
    pthread_t *thread_backing,
    size_t thread_count,
    CoreWorkerTask *task_backing,
    size_t task_capacity,
    CoreQueueMutex *completion_queue);

bool core_workers_submit(CoreWorkers *workers, CoreWorkerTaskFn fn, void *task_ctx);
void core_workers_shutdown_with_mode(CoreWorkers *workers, CoreWorkersShutdownMode mode);
void core_workers_shutdown(CoreWorkers *workers);
CoreWorkersStats core_workers_stats(CoreWorkers *workers);

#endif
