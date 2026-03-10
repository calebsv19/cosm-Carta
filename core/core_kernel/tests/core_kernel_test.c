#include "core_kernel.h"

#include <assert.h>
#include <stdint.h>

#include "core_time.h"

typedef struct TestCtx {
    int init_count;
    int event_count;
    int worker_msg_count;
    int timer_count;
    int update_count;
    int shutdown_count;
    bool render_hint;
} TestCtx;

static bool on_init(void *ctx) {
    TestCtx *s = (TestCtx *)ctx;
    s->init_count++;
    return true;
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
    assert(core_kernel_init(&kernel, NULL, &sched, &jobs, &wake, modules, 8));

    void *event_slots[16] = {0};
    void *worker_slots[16] = {0};
    CoreQueueMutex event_q;
    CoreQueueMutex worker_q;
    assert(core_queue_mutex_init(&event_q, event_slots, 16));
    assert(core_queue_mutex_init(&worker_q, worker_slots, 16));
    core_kernel_bind_queues(&kernel, &event_q, &worker_q);

    TestCtx ctx = {0};
    ctx.render_hint = true;

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

    core_queue_mutex_destroy(&event_q);
    core_queue_mutex_destroy(&worker_q);
    core_wake_shutdown(&wake);

    return 0;
}
