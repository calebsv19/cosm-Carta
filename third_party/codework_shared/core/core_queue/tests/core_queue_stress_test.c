#include "core_queue.h"

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>

#define PRODUCERS 4
#define ITEMS_PER_PRODUCER 4000

typedef struct ProducerCtx {
    CoreQueueMutex *q;
    int producer_id;
} ProducerCtx;

static atomic_int g_produced = 0;
static atomic_int g_consumed = 0;

static void *producer_main(void *arg) {
    ProducerCtx *ctx = (ProducerCtx *)arg;
    (void)ctx->producer_id;

    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        uintptr_t value = (uintptr_t)(i + 1);
        while (!core_queue_mutex_push(ctx->q, (void *)value)) {
            /* retry on full queue */
        }
        atomic_fetch_add_explicit(&g_produced, 1, memory_order_relaxed);
    }

    return NULL;
}

int main(void) {
    void *backing[2048] = {0};
    CoreQueueMutex q;
    assert(core_queue_mutex_init_ex(&q, backing, 2048, CORE_QUEUE_OVERFLOW_REJECT));

    pthread_t threads[PRODUCERS] = {0};
    ProducerCtx ctx[PRODUCERS] = {0};
    for (int i = 0; i < PRODUCERS; ++i) {
        ctx[i].q = &q;
        ctx[i].producer_id = i;
        assert(pthread_create(&threads[i], NULL, producer_main, &ctx[i]) == 0);
    }

    const int target = PRODUCERS * ITEMS_PER_PRODUCER;
    while (atomic_load_explicit(&g_consumed, memory_order_relaxed) < target) {
        void *out = NULL;
        if (core_queue_mutex_timed_pop(&q, &out, 10)) {
            assert((uintptr_t)out > 0);
            atomic_fetch_add_explicit(&g_consumed, 1, memory_order_relaxed);
        }
    }

    for (int i = 0; i < PRODUCERS; ++i) {
        pthread_join(threads[i], NULL);
    }

    assert(atomic_load_explicit(&g_produced, memory_order_relaxed) == target);
    assert(atomic_load_explicit(&g_consumed, memory_order_relaxed) == target);

    core_queue_mutex_destroy(&q);
    return 0;
}
