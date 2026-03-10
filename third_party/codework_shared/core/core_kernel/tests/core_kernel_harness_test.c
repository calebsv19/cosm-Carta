#include "core_kernel.h"

#include <assert.h>
#include <stdint.h>

#include "core_time.h"
#include "core_workers.h"

typedef struct HarnessCtx {
    CoreKernel *kernel;
    CoreWake *wake;
    int timer_count;
    int worker_msg_count;
    int update_count;
} HarnessCtx;

typedef struct WorkTaskCtx {
    CoreWake *wake;
    int value;
} WorkTaskCtx;

static void timer_cb(uint64_t id, void *user_ctx) {
    HarnessCtx *ctx = (HarnessCtx *)user_ctx;
    core_kernel_notify_timer(ctx->kernel, id);
}

static void *worker_task(void *task_ctx) {
    WorkTaskCtx *ctx = (WorkTaskCtx *)task_ctx;
    core_wake_signal(ctx->wake);
    return (void *)(intptr_t)(ctx->value + 100);
}

static void on_timer(void *module_ctx, uint64_t timer_id) {
    (void)timer_id;
    HarnessCtx *ctx = (HarnessCtx *)module_ctx;
    ctx->timer_count++;
}

static void on_worker_msg(void *module_ctx, void *msg) {
    (void)msg;
    HarnessCtx *ctx = (HarnessCtx *)module_ctx;
    ctx->worker_msg_count++;
}

static void on_update(void *module_ctx, uint64_t now_ns) {
    (void)now_ns;
    HarnessCtx *ctx = (HarnessCtx *)module_ctx;
    ctx->update_count++;
}

static bool fake_now(void *ctx, uint64_t *out_now_ns) {
    uint64_t *now = (uint64_t *)ctx;
    *out_now_ns = *now;
    *now += 1000000ULL; /* 1 ms tick */
    return true;
}

int main(void) {
    uint64_t fake_clock = 0;
    CoreTimeProvider tp = {fake_now, &fake_clock};
    assert(core_time_set_provider(tp));

    CoreWake wake;
    assert(core_wake_init_cond(&wake));

    CoreSchedTimer sched_backing[128] = {0};
    CoreSched sched;
    assert(core_sched_init(&sched, sched_backing, 128));

    CoreJob job_backing[64] = {0};
    CoreJobs jobs;
    assert(core_jobs_init(&jobs, job_backing, 64));

    void *event_slots[64] = {0};
    void *worker_slots[512] = {0};
    CoreQueueMutex event_q;
    CoreQueueMutex worker_q;
    assert(core_queue_mutex_init(&event_q, event_slots, 64));
    assert(core_queue_mutex_init(&worker_q, worker_slots, 512));

    CoreKernelPolicy policy = {
        CORE_KERNEL_IDLE_BLOCK,
        60,
        2,
        1,
        2,
        true
    };

    CoreKernelModule modules[8] = {0};
    CoreKernel kernel;
    assert(core_kernel_init(&kernel, &policy, &sched, &jobs, &wake, modules, 8));
    core_kernel_bind_queues(&kernel, &event_q, &worker_q);

    HarnessCtx h = {&kernel, &wake, 0, 0, 0};
    CoreKernelModuleHooks hooks = {0};
    hooks.on_timer = on_timer;
    hooks.on_worker_msg = on_worker_msg;
    hooks.on_update = on_update;
    assert(core_kernel_register_module(&kernel, hooks, &h));

    assert(core_sched_add_timer(&sched, 2 * 1000000ULL, 2 * 1000000ULL, timer_cb, &h) != 0);

    pthread_t worker_threads[2] = {0};
    CoreWorkerTask worker_tasks[128] = {0};
    CoreWorkers workers;
    assert(core_workers_init(&workers, worker_threads, 2, worker_tasks, 128, &worker_q));

    WorkTaskCtx tasks[32];
    for (int i = 0; i < 32; ++i) {
        tasks[i].wake = &wake;
        tasks[i].value = i;
        assert(core_workers_submit(&workers, worker_task, &tasks[i]));
    }

    for (int i = 0; i < 80; ++i) {
        core_kernel_tick(&kernel, core_time_now_ns());
    }

    assert(h.timer_count > 0);
    assert(h.worker_msg_count > 0);
    assert(h.update_count > 0);

    core_workers_shutdown(&workers);
    core_kernel_shutdown(&kernel);

    core_queue_mutex_destroy(&worker_q);
    core_queue_mutex_destroy(&event_q);
    core_wake_shutdown(&wake);
    core_time_reset_provider();

    return 0;
}
