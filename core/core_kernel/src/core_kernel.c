/*
 * core_kernel.c
 * Part of the CodeWork Shared Libraries
 * Copyright (c) 2026 Caleb S. V.
 * Licensed under the Apache License, Version 2.0
 */

#include "core_kernel.h"

#include "core_time.h"

static const CoreKernelPolicy k_default_policy = {
    CORE_KERNEL_IDLE_BLOCK,
    60,
    4,
    16,
    0,
    true
};

bool core_kernel_init(
    CoreKernel *kernel,
    const CoreKernelPolicy *policy,
    CoreSched *sched,
    CoreJobs *jobs,
    CoreWake *wake,
    CoreKernelModule *module_backing,
    size_t module_capacity) {
    if (!kernel || !sched || !jobs || !wake || !module_backing || module_capacity == 0) {
        return false;
    }

    kernel->policy = policy ? *policy : k_default_policy;
    kernel->sched = sched;
    kernel->jobs = jobs;
    kernel->wake = wake;
    kernel->event_queue = NULL;
    kernel->worker_msg_queue = NULL;
    kernel->modules = module_backing;
    kernel->module_capacity = module_capacity;
    kernel->module_count = 0;
    kernel->running = true;
    kernel->render_requested = false;
    kernel->last_tick_work_units = 0;
    return true;
}

void core_kernel_bind_queues(
    CoreKernel *kernel,
    CoreQueueMutex *event_queue,
    CoreQueueMutex *worker_msg_queue) {
    if (!kernel) return;
    kernel->event_queue = event_queue;
    kernel->worker_msg_queue = worker_msg_queue;
}

bool core_kernel_register_module(
    CoreKernel *kernel,
    CoreKernelModuleHooks hooks,
    void *module_ctx) {
    if (!kernel || kernel->module_count >= kernel->module_capacity) return false;
    CoreKernelModule *m = &kernel->modules[kernel->module_count++];
    m->hooks = hooks;
    m->module_ctx = module_ctx;

    if (m->hooks.on_init) {
        return m->hooks.on_init(module_ctx);
    }
    return true;
}

void core_kernel_notify_timer(CoreKernel *kernel, uint64_t timer_id) {
    if (!kernel || !kernel->running) return;
    for (size_t i = 0; i < kernel->module_count; ++i) {
        CoreKernelModule *m = &kernel->modules[i];
        if (m->hooks.on_timer) {
            m->hooks.on_timer(m->module_ctx, timer_id);
        }
    }
}

static uint64_t drain_event_queue(CoreKernel *kernel) {
    if (!kernel->event_queue) return 0;

    uint64_t n = 0;
    void *evt = NULL;
    while (core_queue_mutex_pop(kernel->event_queue, &evt)) {
        for (size_t i = 0; i < kernel->module_count; ++i) {
            CoreKernelModule *m = &kernel->modules[i];
            if (m->hooks.on_event) {
                m->hooks.on_event(m->module_ctx, evt);
            }
        }
        n++;
    }

    return n;
}

static uint64_t drain_worker_queue(CoreKernel *kernel) {
    if (!kernel->worker_msg_queue) return 0;

    uint64_t n = 0;
    void *msg = NULL;
    while (core_queue_mutex_pop(kernel->worker_msg_queue, &msg)) {
        for (size_t i = 0; i < kernel->module_count; ++i) {
            CoreKernelModule *m = &kernel->modules[i];
            if (m->hooks.on_worker_msg) {
                m->hooks.on_worker_msg(m->module_ctx, msg);
            }
        }
        n++;
    }

    return n;
}

static void apply_idle_policy(CoreKernel *kernel, uint64_t work_units) {
    if (work_units > 0) {
        return;
    }

    if (kernel->policy.idle_mode == CORE_KERNEL_IDLE_SPIN) {
        return;
    }

    uint32_t timeout_ms = kernel->policy.max_idle_timeout_ms;
    if (timeout_ms == 0) {
        timeout_ms = 1;
    }

    if (kernel->policy.idle_mode == CORE_KERNEL_IDLE_BACKOFF) {
        timeout_ms = timeout_ms / 4u;
        if (timeout_ms == 0) timeout_ms = 1;
    }

    (void)core_wake_wait(kernel->wake, timeout_ms);
}

void core_kernel_tick(CoreKernel *kernel, uint64_t now_ns) {
    if (!kernel || !kernel->running) return;

    if (now_ns == 0) {
        now_ns = core_time_now_ns();
    }

    uint64_t work = 0;

    work += drain_event_queue(kernel);
    work += drain_worker_queue(kernel);
    work += core_sched_fire_due(kernel->sched, now_ns, 1024);
    work += core_jobs_run_budget(kernel->jobs, kernel->policy.job_budget_ms);

    bool render_requested = false;
    for (size_t i = 0; i < kernel->module_count; ++i) {
        CoreKernelModule *m = &kernel->modules[i];
        if (m->hooks.on_update) {
            m->hooks.on_update(m->module_ctx, now_ns);
        }
        if (m->hooks.on_render_hint && m->hooks.on_render_hint(m->module_ctx)) {
            render_requested = true;
        }
    }

    kernel->render_requested = render_requested;
    kernel->last_tick_work_units = work;

    apply_idle_policy(kernel, work);
}

bool core_kernel_render_requested(const CoreKernel *kernel) {
    if (!kernel) return false;
    return kernel->render_requested;
}

uint64_t core_kernel_last_tick_work_units(const CoreKernel *kernel) {
    if (!kernel) return 0;
    return kernel->last_tick_work_units;
}

void core_kernel_shutdown(CoreKernel *kernel) {
    if (!kernel || !kernel->running) return;

    for (size_t i = 0; i < kernel->module_count; ++i) {
        CoreKernelModule *m = &kernel->modules[i];
        if (m->hooks.on_shutdown) {
            m->hooks.on_shutdown(m->module_ctx);
        }
    }

    kernel->running = false;
}
