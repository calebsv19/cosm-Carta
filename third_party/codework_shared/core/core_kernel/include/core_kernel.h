#ifndef CORE_KERNEL_H
#define CORE_KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "core_jobs.h"
#include "core_queue.h"
#include "core_sched.h"
#include "core_wake.h"

typedef enum CoreKernelIdleMode {
    CORE_KERNEL_IDLE_BLOCK = 0,
    CORE_KERNEL_IDLE_SPIN = 1,
    CORE_KERNEL_IDLE_BACKOFF = 2
} CoreKernelIdleMode;

typedef struct CoreKernelPolicy {
    CoreKernelIdleMode idle_mode;
    uint32_t frame_cap_hz;
    uint32_t job_budget_ms;
    uint32_t max_idle_timeout_ms;
    uint32_t worker_thread_count;
    bool coalesce_input_events;
} CoreKernelPolicy;

typedef struct CoreKernelModuleHooks {
    bool (*on_init)(void *module_ctx);
    void (*on_event)(void *module_ctx, const void *event_data);
    void (*on_timer)(void *module_ctx, uint64_t timer_id);
    void (*on_worker_msg)(void *module_ctx, void *msg);
    void (*on_update)(void *module_ctx, uint64_t now_ns);
    bool (*on_render_hint)(void *module_ctx);
    void (*on_shutdown)(void *module_ctx);
} CoreKernelModuleHooks;

typedef struct CoreKernelModule {
    CoreKernelModuleHooks hooks;
    void *module_ctx;
} CoreKernelModule;

typedef struct CoreKernel {
    CoreKernelPolicy policy;
    CoreSched *sched;
    CoreJobs *jobs;
    CoreWake *wake;

    CoreQueueMutex *event_queue;
    CoreQueueMutex *worker_msg_queue;

    CoreKernelModule *modules;
    size_t module_capacity;
    size_t module_count;

    bool running;
    bool render_requested;
    uint64_t last_tick_work_units;
} CoreKernel;

bool core_kernel_init(
    CoreKernel *kernel,
    const CoreKernelPolicy *policy,
    CoreSched *sched,
    CoreJobs *jobs,
    CoreWake *wake,
    CoreKernelModule *module_backing,
    size_t module_capacity);

void core_kernel_bind_queues(
    CoreKernel *kernel,
    CoreQueueMutex *event_queue,
    CoreQueueMutex *worker_msg_queue);

bool core_kernel_register_module(
    CoreKernel *kernel,
    CoreKernelModuleHooks hooks,
    void *module_ctx);

void core_kernel_notify_timer(CoreKernel *kernel, uint64_t timer_id);

void core_kernel_tick(CoreKernel *kernel, uint64_t now_ns);
bool core_kernel_render_requested(const CoreKernel *kernel);
uint64_t core_kernel_last_tick_work_units(const CoreKernel *kernel);
void core_kernel_shutdown(CoreKernel *kernel);

#endif
