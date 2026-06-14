#include "core_kernel.h"

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct TestCtx {
    int init_count;
    int event_count;
    int worker_msg_count;
    int timer_count;
    int update_count;
    int shutdown_count;
    bool render_hint;
    bool init_ok;
} TestCtx;

typedef struct WakeProbe {
    int signal_calls;
    int wait_calls;
    uint32_t last_timeout_ms;
    bool signal_ok;
    CoreWakeWaitResult wait_result;
} WakeProbe;

static bool on_init(void *ctx) {
    TestCtx *s = (TestCtx *)ctx;
    s->init_count++;
    return s->init_ok;
}

static void on_event(void *ctx, const void *event_data) {
    (void)event_data;
    TestCtx *s = (TestCtx *)ctx;
    s->event_count++;
}

static void on_worker_msg(void *ctx, void *msg) {
    (void)msg;
    TestCtx *s = (TestCtx *)ctx;
    s->worker_msg_count++;
}

static void on_update(void *ctx, uint64_t now_ns) {
    (void)now_ns;
    TestCtx *s = (TestCtx *)ctx;
    s->update_count++;
}

static bool on_render_hint(void *ctx) {
    TestCtx *s = (TestCtx *)ctx;
    return s->render_hint;
}

static void on_shutdown(void *ctx) {
    TestCtx *s = (TestCtx *)ctx;
    s->shutdown_count++;
}

static void timer_cb(uint64_t id, void *user_ctx) {
    (void)id;
    TestCtx *s = (TestCtx *)user_ctx;
    s->timer_count++;
}

static void job_cb(void *user_ctx) {
    TestCtx *s = (TestCtx *)user_ctx;
    s->update_count += 10;
}

static bool wake_probe_signal(void *ctx) {
    WakeProbe *probe = (WakeProbe *)ctx;
    probe->signal_calls++;
    return probe->signal_ok;
}

static CoreWakeWaitResult wake_probe_wait(void *ctx, uint32_t timeout_ms) {
    WakeProbe *probe = (WakeProbe *)ctx;
    probe->wait_calls++;
    probe->last_timeout_ms = timeout_ms;
    return probe->wait_result;
}

int main(void) {
    CoreSchedTimer timer_backing[16] = {0};
    CoreSched sched;
    assert(core_sched_init(&sched, timer_backing, 16));

    CoreJob job_backing[16] = {0};
    CoreJobs jobs;
    assert(core_jobs_init(&jobs, job_backing, 16));

    CoreWake wake;
    assert(core_wake_init_cond(&wake));

    CoreKernelModule modules[8] = {0};
    CoreKernel kernel;

    assert(!core_kernel_init(NULL, NULL, &sched, &jobs, &wake, modules, 8));
    assert(!core_kernel_init(&kernel, NULL, NULL, &jobs, &wake, modules, 8));
    assert(!core_kernel_init(&kernel, NULL, &sched, NULL, &wake, modules, 8));
    assert(!core_kernel_init(&kernel, NULL, &sched, &jobs, NULL, modules, 8));
    assert(!core_kernel_init(&kernel, NULL, &sched, &jobs, &wake, NULL, 8));
    assert(!core_kernel_init(&kernel, NULL, &sched, &jobs, &wake, modules, 0));
    assert(!core_kernel_register_module(NULL, (CoreKernelModuleHooks){0}, NULL));
    assert(!core_kernel_render_requested(NULL));
    assert(core_kernel_last_tick_work_units(NULL) == 0);

    assert(core_kernel_init(&kernel, NULL, &sched, &jobs, &wake, modules, 8));

    void *event_slots[16] = {0};
    void *worker_slots[16] = {0};
    CoreQueueMutex event_q;
    CoreQueueMutex worker_q;
    assert(core_queue_mutex_init(&event_q, event_slots, 16));
    assert(core_queue_mutex_init(&worker_q, worker_slots, 16));
    core_kernel_bind_queues(&kernel, &event_q, &worker_q);

    TestCtx failed_ctx = {0};
    failed_ctx.init_ok = false;
    CoreKernelModuleHooks failing_hooks = {0};
    failing_hooks.on_init = on_init;
    assert(!core_kernel_register_module(&kernel, failing_hooks, &failed_ctx));
    assert(failed_ctx.init_count == 1);
    assert(kernel.module_count == 0);

    TestCtx ctx = {0};
    ctx.render_hint = true;
    ctx.init_ok = true;

    CoreKernelModuleHooks hooks = {0};
    hooks.on_init = on_init;
    hooks.on_event = on_event;
    hooks.on_worker_msg = on_worker_msg;
    hooks.on_update = on_update;
    hooks.on_render_hint = on_render_hint;
    hooks.on_shutdown = on_shutdown;
    assert(core_kernel_register_module(&kernel, hooks, &ctx));

    assert(core_sched_add_timer(&sched, 10, 0, timer_cb, &ctx) != 0);
    assert(core_jobs_enqueue(&jobs, job_cb, &ctx));
    assert(core_queue_mutex_push(&event_q, (void *)(intptr_t)1));
    assert(core_queue_mutex_push(&worker_q, (void *)(intptr_t)2));

    core_kernel_tick(&kernel, 10);

    assert(ctx.init_count == 1);
    assert(ctx.event_count == 1);
    assert(ctx.worker_msg_count == 1);
    assert(ctx.timer_count == 1);
    assert(ctx.update_count >= 11);
    assert(core_kernel_render_requested(&kernel));
    assert(core_kernel_last_tick_work_units(&kernel) >= 4);

    core_kernel_shutdown(&kernel);
    assert(ctx.shutdown_count == 1);
    core_kernel_shutdown(&kernel);
    assert(ctx.shutdown_count == 1);
    assert(!core_kernel_register_module(&kernel, hooks, &ctx));
    core_kernel_tick(&kernel, 10);
    assert(ctx.event_count == 1);
    assert(ctx.update_count >= 11);

    core_queue_mutex_destroy(&event_q);
    core_queue_mutex_destroy(&worker_q);
    core_wake_shutdown(&wake);

    CoreSchedTimer cap_timer_backing[2] = {0};
    CoreSched cap_sched;
    assert(core_sched_init(&cap_sched, cap_timer_backing, 2));

    CoreJob cap_job_backing[2] = {0};
    CoreJobs cap_jobs;
    assert(core_jobs_init(&cap_jobs, cap_job_backing, 2));

    CoreWake cap_wake;
    assert(core_wake_init_cond(&cap_wake));

    CoreKernelModule one_module[1] = {0};
    CoreKernel cap_kernel;
    assert(core_kernel_init(&cap_kernel, NULL, &cap_sched, &cap_jobs, &cap_wake, one_module, 1));
    TestCtx cap_ctx = {0};
    cap_ctx.init_ok = true;
    assert(core_kernel_register_module(&cap_kernel, hooks, &cap_ctx));
    assert(!core_kernel_register_module(&cap_kernel, hooks, &cap_ctx));
    core_kernel_shutdown(&cap_kernel);
    core_wake_shutdown(&cap_wake);

    WakeProbe block_probe = {0, 0, 0, true, CORE_WAKE_WAIT_TIMEOUT};
    CoreWake block_wake;
    assert(core_wake_init_external(
        &block_wake,
        wake_probe_signal,
        wake_probe_wait,
        &block_probe));

    CoreKernelPolicy invalid_idle_policy = {
        (CoreKernelIdleMode)99,
        60,
        4,
        8,
        0,
        true
    };
    CoreSchedTimer block_timer_backing[2] = {0};
    CoreSched block_sched;
    assert(core_sched_init(&block_sched, block_timer_backing, 2));
    CoreJob block_job_backing[2] = {0};
    CoreJobs block_jobs;
    assert(core_jobs_init(&block_jobs, block_job_backing, 2));
    CoreKernelModule block_modules[1] = {0};
    CoreKernel block_kernel;
    assert(core_kernel_init(
        &block_kernel,
        &invalid_idle_policy,
        &block_sched,
        &block_jobs,
        &block_wake,
        block_modules,
        1));
    assert(block_kernel.policy.idle_mode == CORE_KERNEL_IDLE_BLOCK);
    core_kernel_tick(&block_kernel, 123);
    assert(block_probe.wait_calls == 1);
    assert(block_probe.last_timeout_ms == 8);
    core_kernel_shutdown(&block_kernel);
    core_wake_shutdown(&block_wake);

    WakeProbe backoff_probe = {0, 0, 0, true, CORE_WAKE_WAIT_TIMEOUT};
    CoreWake backoff_wake;
    assert(core_wake_init_external(
        &backoff_wake,
        wake_probe_signal,
        wake_probe_wait,
        &backoff_probe));

    CoreKernelPolicy backoff_policy = {
        CORE_KERNEL_IDLE_BACKOFF,
        60,
        4,
        8,
        0,
        true
    };
    CoreSchedTimer backoff_timer_backing[2] = {0};
    CoreSched backoff_sched;
    assert(core_sched_init(&backoff_sched, backoff_timer_backing, 2));
    CoreJob backoff_job_backing[2] = {0};
    CoreJobs backoff_jobs;
    assert(core_jobs_init(&backoff_jobs, backoff_job_backing, 2));
    CoreKernelModule backoff_modules[1] = {0};
    CoreKernel backoff_kernel;
    assert(core_kernel_init(
        &backoff_kernel,
        &backoff_policy,
        &backoff_sched,
        &backoff_jobs,
        &backoff_wake,
        backoff_modules,
        1));
    core_kernel_tick(&backoff_kernel, 123);
    assert(backoff_probe.wait_calls == 1);
    assert(backoff_probe.last_timeout_ms == 2);
    core_kernel_shutdown(&backoff_kernel);
    core_wake_shutdown(&backoff_wake);

    return 0;
}
