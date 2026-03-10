#include "core_wake.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>

#define SIGNAL_COUNT 2000

typedef struct WaitCtx {
    CoreWake *wake;
    atomic_int seen;
} WaitCtx;

static void *waiter_main(void *arg) {
    WaitCtx *ctx = (WaitCtx *)arg;
    for (int i = 0; i < SIGNAL_COUNT; ++i) {
        CoreWakeWaitResult r = core_wake_wait(ctx->wake, CORE_WAKE_TIMEOUT_INFINITE);
        if (r == CORE_WAKE_WAIT_SIGNALED) {
            atomic_fetch_add_explicit(&ctx->seen, 1, memory_order_relaxed);
        }
    }
    return NULL;
}

int main(void) {
    CoreWake wake;
    assert(core_wake_init_cond(&wake));

    WaitCtx ctx = {&wake, 0};
    pthread_t waiter;
    assert(pthread_create(&waiter, NULL, waiter_main, &ctx) == 0);

    for (int i = 0; i < SIGNAL_COUNT; ++i) {
        assert(core_wake_signal(&wake));
    }

    pthread_join(waiter, NULL);
    assert(atomic_load_explicit(&ctx.seen, memory_order_relaxed) == SIGNAL_COUNT);

    core_wake_shutdown(&wake);
    return 0;
}
