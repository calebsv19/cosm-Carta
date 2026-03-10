/*
 * core_queue.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_queue.h"

#include <errno.h>
#include <time.h>

bool core_queue_ring_init(CoreQueueRing *q, void **backing, size_t capacity) {
    return core_queue_ring_init_ex(q, backing, capacity, CORE_QUEUE_OVERFLOW_REJECT);
}

bool core_queue_ring_init_ex(
    CoreQueueRing *q,
    void **backing,
    size_t capacity,
    CoreQueueOverflowPolicy overflow_policy) {
    if (!q || !backing || capacity == 0) return false;
    q->items = backing;
    q->capacity = capacity;
    q->head = 0;
    q->tail = 0;
    q->count = 0;
    q->overflow_policy = overflow_policy;
    q->stats = (CoreQueueStats){0};
    return true;
}

bool core_queue_ring_push(CoreQueueRing *q, void *item) {
    if (!q) return false;

    if (q->count == q->capacity) {
        if (q->overflow_policy == CORE_QUEUE_OVERFLOW_DROP_OLDEST) {
            q->head = (q->head + 1u) % q->capacity;
            q->count--;
            q->stats.drops++;
        } else {
            q->stats.push_fail++;
            return false;
        }
    }

    q->items[q->tail] = item;
    q->tail = (q->tail + 1u) % q->capacity;
    q->count++;
    q->stats.push_ok++;
    return true;
}

bool core_queue_ring_pop(CoreQueueRing *q, void **out_item) {
    if (!q || !out_item) return false;
    if (q->count == 0) {
        q->stats.pop_empty++;
        return false;
    }

    *out_item = q->items[q->head];
    q->head = (q->head + 1u) % q->capacity;
    q->count--;
    q->stats.pop_ok++;
    return true;
}

size_t core_queue_ring_size(const CoreQueueRing *q) {
    if (!q) return 0;
    return q->count;
}

CoreQueueStats core_queue_ring_stats(const CoreQueueRing *q) {
    CoreQueueStats empty = {0};
    if (!q) return empty;
    return q->stats;
}

bool core_queue_mutex_init(CoreQueueMutex *q, void **backing, size_t capacity) {
    return core_queue_mutex_init_ex(q, backing, capacity, CORE_QUEUE_OVERFLOW_REJECT);
}

bool core_queue_mutex_init_ex(
    CoreQueueMutex *q,
    void **backing,
    size_t capacity,
    CoreQueueOverflowPolicy overflow_policy) {
    if (!q) return false;
    if (pthread_mutex_init(&q->mu, NULL) != 0) return false;
    if (pthread_cond_init(&q->cv, NULL) != 0) {
        pthread_mutex_destroy(&q->mu);
        return false;
    }
    if (!core_queue_ring_init_ex(&q->ring, backing, capacity, overflow_policy)) {
        pthread_cond_destroy(&q->cv);
        pthread_mutex_destroy(&q->mu);
        return false;
    }
    return true;
}

void core_queue_mutex_destroy(CoreQueueMutex *q) {
    if (!q) return;
    pthread_cond_destroy(&q->cv);
    pthread_mutex_destroy(&q->mu);
}

bool core_queue_mutex_push(CoreQueueMutex *q, void *item) {
    if (!q) return false;
    pthread_mutex_lock(&q->mu);
    bool ok = core_queue_ring_push(&q->ring, item);
    if (ok) {
        pthread_cond_signal(&q->cv);
    }
    pthread_mutex_unlock(&q->mu);
    return ok;
}

bool core_queue_mutex_pop(CoreQueueMutex *q, void **out_item) {
    if (!q) return false;
    pthread_mutex_lock(&q->mu);
    bool ok = core_queue_ring_pop(&q->ring, out_item);
    pthread_mutex_unlock(&q->mu);
    return ok;
}

bool core_queue_mutex_timed_pop(CoreQueueMutex *q, void **out_item, uint32_t timeout_ms) {
    if (!q || !out_item) return false;

    pthread_mutex_lock(&q->mu);
    if (q->ring.count == 0) {
        struct timespec ts;
        if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
            pthread_mutex_unlock(&q->mu);
            return false;
        }
        ts.tv_sec += (time_t)(timeout_ms / 1000u);
        ts.tv_nsec += (long)((timeout_ms % 1000u) * 1000000u);
        if (ts.tv_nsec >= 1000000000L) {
            ts.tv_sec += 1;
            ts.tv_nsec -= 1000000000L;
        }

        int rc = 0;
        while (q->ring.count == 0 && rc != ETIMEDOUT) {
            rc = pthread_cond_timedwait(&q->cv, &q->mu, &ts);
        }

        if (q->ring.count == 0) {
            q->ring.stats.pop_empty++;
            pthread_mutex_unlock(&q->mu);
            return false;
        }
    }

    bool ok = core_queue_ring_pop(&q->ring, out_item);
    pthread_mutex_unlock(&q->mu);
    return ok;
}

size_t core_queue_mutex_size(CoreQueueMutex *q) {
    if (!q) return 0;
    pthread_mutex_lock(&q->mu);
    size_t n = q->ring.count;
    pthread_mutex_unlock(&q->mu);
    return n;
}

CoreQueueStats core_queue_mutex_stats(CoreQueueMutex *q) {
    CoreQueueStats empty = {0};
    if (!q) return empty;
    pthread_mutex_lock(&q->mu);
    CoreQueueStats s = q->ring.stats;
    pthread_mutex_unlock(&q->mu);
    return s;
}
