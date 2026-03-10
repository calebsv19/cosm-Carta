/*
 * core_wake.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_wake.h"

#include <errno.h>
#include <time.h>

bool core_wake_init_cond(CoreWake *wake) {
    if (!wake) return false;
    wake->backend = CORE_WAKE_BACKEND_COND;
    wake->pending_signals = 0;
    wake->external_signal = NULL;
    wake->external_wait = NULL;
    wake->external_ctx = NULL;
    wake->initialized = false;

    if (pthread_mutex_init(&wake->mu, NULL) != 0) return false;
    if (pthread_cond_init(&wake->cv, NULL) != 0) {
        pthread_mutex_destroy(&wake->mu);
        return false;
    }

    wake->initialized = true;
    return true;
}

bool core_wake_init_external(
    CoreWake *wake,
    CoreWakeExternalSignalFn signal_fn,
    CoreWakeExternalWaitFn wait_fn,
    void *ctx) {
    if (!wake || !signal_fn || !wait_fn) return false;

    wake->backend = CORE_WAKE_BACKEND_EXTERNAL;
    wake->pending_signals = 0;
    wake->external_signal = signal_fn;
    wake->external_wait = wait_fn;
    wake->external_ctx = ctx;
    wake->initialized = true;
    return true;
}

bool core_wake_signal(CoreWake *wake) {
    if (!wake || !wake->initialized) return false;

    if (wake->backend == CORE_WAKE_BACKEND_EXTERNAL) {
        return wake->external_signal(wake->external_ctx);
    }

    pthread_mutex_lock(&wake->mu);
    wake->pending_signals++;
    pthread_cond_signal(&wake->cv);
    pthread_mutex_unlock(&wake->mu);
    return true;
}

CoreWakeWaitResult core_wake_wait(CoreWake *wake, uint32_t timeout_ms) {
    if (!wake || !wake->initialized) return CORE_WAKE_WAIT_ERROR;

    if (wake->backend == CORE_WAKE_BACKEND_EXTERNAL) {
        return wake->external_wait(wake->external_ctx, timeout_ms);
    }

    pthread_mutex_lock(&wake->mu);
    if (wake->pending_signals > 0) {
        wake->pending_signals--;
        pthread_mutex_unlock(&wake->mu);
        return CORE_WAKE_WAIT_SIGNALED;
    }

    if (timeout_ms == CORE_WAKE_TIMEOUT_INFINITE) {
        while (wake->pending_signals == 0) {
            if (pthread_cond_wait(&wake->cv, &wake->mu) != 0) {
                pthread_mutex_unlock(&wake->mu);
                return CORE_WAKE_WAIT_ERROR;
            }
        }
        wake->pending_signals--;
        pthread_mutex_unlock(&wake->mu);
        return CORE_WAKE_WAIT_SIGNALED;
    }

    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
        pthread_mutex_unlock(&wake->mu);
        return CORE_WAKE_WAIT_ERROR;
    }

    ts.tv_sec += (time_t)(timeout_ms / 1000u);
    ts.tv_nsec += (long)((timeout_ms % 1000u) * 1000000u);
    if (ts.tv_nsec >= 1000000000L) {
        ts.tv_sec += 1;
        ts.tv_nsec -= 1000000000L;
    }

    while (wake->pending_signals == 0) {
        int rc = pthread_cond_timedwait(&wake->cv, &wake->mu, &ts);
        if (rc == ETIMEDOUT) {
            pthread_mutex_unlock(&wake->mu);
            return CORE_WAKE_WAIT_TIMEOUT;
        }
        if (rc != 0) {
            pthread_mutex_unlock(&wake->mu);
            return CORE_WAKE_WAIT_ERROR;
        }
    }

    wake->pending_signals--;
    pthread_mutex_unlock(&wake->mu);
    return CORE_WAKE_WAIT_SIGNALED;
}

void core_wake_shutdown(CoreWake *wake) {
    if (!wake || !wake->initialized) return;

    if (wake->backend == CORE_WAKE_BACKEND_COND) {
        pthread_cond_destroy(&wake->cv);
        pthread_mutex_destroy(&wake->mu);
    }

    wake->initialized = false;
}
