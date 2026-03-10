/*
 * core_workers.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_workers.h"

static bool pop_task_locked(CoreWorkers *workers, CoreWorkerTask *out) {
    if (workers->task_count == 0) return false;
    *out = workers->task_slots[workers->task_head];
    workers->task_head = (workers->task_head + 1u) % workers->task_capacity;
    workers->task_count--;
    return true;
}

static void *worker_main(void *arg) {
    CoreWorkers *workers = (CoreWorkers *)arg;

    while (true) {
        pthread_mutex_lock(&workers->mu);
        while (!workers->stop && workers->task_count == 0) {
            pthread_cond_wait(&workers->cv, &workers->mu);
        }

        if (workers->stop) {
            if (workers->cancel_pending) {
                pthread_mutex_unlock(&workers->mu);
                break;
            }
            if (workers->task_count == 0) {
                pthread_mutex_unlock(&workers->mu);
                break;
            }
        }

        CoreWorkerTask task = {0};
        bool ok = pop_task_locked(workers, &task);
        pthread_mutex_unlock(&workers->mu);

        if (!ok || !task.fn) {
            continue;
        }

        void *completion = task.fn(task.task_ctx);

        pthread_mutex_lock(&workers->mu);
        workers->stats.completed++;
        pthread_mutex_unlock(&workers->mu);

        if (workers->completion_queue && completion) {
            core_queue_mutex_push(workers->completion_queue, completion);
        }
    }

    return NULL;
}

bool core_workers_init(
    CoreWorkers *workers,
    pthread_t *thread_backing,
    size_t thread_count,
    CoreWorkerTask *task_backing,
    size_t task_capacity,
    CoreQueueMutex *completion_queue) {
    if (!workers || !thread_backing || thread_count == 0 || !task_backing || task_capacity == 0) {
        return false;
    }

    workers->threads = thread_backing;
    workers->thread_count = thread_count;
    workers->task_slots = task_backing;
    workers->task_capacity = task_capacity;
    workers->task_head = 0;
    workers->task_tail = 0;
    workers->task_count = 0;
    workers->stop = false;
    workers->cancel_pending = false;
    workers->completion_queue = completion_queue;
    workers->stats = (CoreWorkersStats){0};
    workers->initialized = false;

    if (pthread_mutex_init(&workers->mu, NULL) != 0) return false;
    if (pthread_cond_init(&workers->cv, NULL) != 0) {
        pthread_mutex_destroy(&workers->mu);
        return false;
    }

    for (size_t i = 0; i < thread_count; ++i) {
        if (pthread_create(&workers->threads[i], NULL, worker_main, workers) != 0) {
            workers->stop = true;
            pthread_cond_broadcast(&workers->cv);
            for (size_t j = 0; j < i; ++j) {
                pthread_join(workers->threads[j], NULL);
            }
            pthread_cond_destroy(&workers->cv);
            pthread_mutex_destroy(&workers->mu);
            return false;
        }
    }

    workers->initialized = true;
    return true;
}

bool core_workers_submit(CoreWorkers *workers, CoreWorkerTaskFn fn, void *task_ctx) {
    if (!workers || !fn) return false;

    pthread_mutex_lock(&workers->mu);
    if (workers->task_count == workers->task_capacity || workers->stop) {
        workers->stats.rejected++;
        pthread_mutex_unlock(&workers->mu);
        return false;
    }

    workers->task_slots[workers->task_tail].fn = fn;
    workers->task_slots[workers->task_tail].task_ctx = task_ctx;
    workers->task_tail = (workers->task_tail + 1u) % workers->task_capacity;
    workers->task_count++;
    workers->stats.submitted++;

    pthread_cond_signal(&workers->cv);
    pthread_mutex_unlock(&workers->mu);
    return true;
}

void core_workers_shutdown_with_mode(CoreWorkers *workers, CoreWorkersShutdownMode mode) {
    if (!workers || !workers->initialized) return;

    pthread_mutex_lock(&workers->mu);
    workers->stop = true;
    workers->cancel_pending = (mode == CORE_WORKERS_SHUTDOWN_CANCEL);
    if (workers->cancel_pending) {
        workers->stats.canceled += (uint64_t)workers->task_count;
        workers->task_count = 0;
        workers->task_head = 0;
        workers->task_tail = 0;
    }
    pthread_cond_broadcast(&workers->cv);
    pthread_mutex_unlock(&workers->mu);

    for (size_t i = 0; i < workers->thread_count; ++i) {
        pthread_join(workers->threads[i], NULL);
    }

    pthread_cond_destroy(&workers->cv);
    pthread_mutex_destroy(&workers->mu);
    workers->initialized = false;
}

void core_workers_shutdown(CoreWorkers *workers) {
    core_workers_shutdown_with_mode(workers, CORE_WORKERS_SHUTDOWN_DRAIN);
}

CoreWorkersStats core_workers_stats(CoreWorkers *workers) {
    CoreWorkersStats empty = {0};
    if (!workers) return empty;
    if (!workers->initialized) return workers->stats;

    pthread_mutex_lock(&workers->mu);
    CoreWorkersStats s = workers->stats;
    pthread_mutex_unlock(&workers->mu);
    return s;
}
