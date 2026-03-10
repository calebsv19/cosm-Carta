#ifndef CORE_WAKE_H
#define CORE_WAKE_H

#include <stdbool.h>
#include <stdint.h>

#include <pthread.h>

typedef enum CoreWakeBackend {
    CORE_WAKE_BACKEND_COND = 1,
    CORE_WAKE_BACKEND_EXTERNAL = 2
} CoreWakeBackend;

typedef enum CoreWakeWaitResult {
    CORE_WAKE_WAIT_SIGNALED = 0,
    CORE_WAKE_WAIT_TIMEOUT = 1,
    CORE_WAKE_WAIT_ERROR = 2
} CoreWakeWaitResult;

#define CORE_WAKE_TIMEOUT_INFINITE UINT32_MAX

typedef bool (*CoreWakeExternalSignalFn)(void *ctx);
typedef CoreWakeWaitResult (*CoreWakeExternalWaitFn)(void *ctx, uint32_t timeout_ms);

typedef struct CoreWake {
    CoreWakeBackend backend;

    pthread_mutex_t mu;
    pthread_cond_t cv;
    uint64_t pending_signals;
    bool initialized;

    CoreWakeExternalSignalFn external_signal;
    CoreWakeExternalWaitFn external_wait;
    void *external_ctx;
} CoreWake;

bool core_wake_init_cond(CoreWake *wake);
bool core_wake_init_external(
    CoreWake *wake,
    CoreWakeExternalSignalFn signal_fn,
    CoreWakeExternalWaitFn wait_fn,
    void *ctx);

bool core_wake_signal(CoreWake *wake);
CoreWakeWaitResult core_wake_wait(CoreWake *wake, uint32_t timeout_ms);
void core_wake_shutdown(CoreWake *wake);

#endif
