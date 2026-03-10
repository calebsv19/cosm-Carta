#include "core_queue.h"

#include <assert.h>

int main(void) {
    void *backing[4] = {0};
    CoreQueueRing q;

    assert(core_queue_ring_init(&q, backing, 4));
    assert(core_queue_ring_push(&q, (void *)1));
    assert(core_queue_ring_push(&q, (void *)2));

    void *out = 0;
    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)1);

    assert(core_queue_ring_push(&q, (void *)3));
    assert(core_queue_ring_push(&q, (void *)4));
    assert(core_queue_ring_push(&q, (void *)5));
    assert(!core_queue_ring_push(&q, (void *)6));

    CoreQueueStats rs = core_queue_ring_stats(&q);
    assert(rs.push_fail == 1);

    CoreQueueRing q_drop;
    assert(core_queue_ring_init_ex(&q_drop, backing, 4, CORE_QUEUE_OVERFLOW_DROP_OLDEST));
    assert(core_queue_ring_push(&q_drop, (void *)10));
    assert(core_queue_ring_push(&q_drop, (void *)11));
    assert(core_queue_ring_push(&q_drop, (void *)12));
    assert(core_queue_ring_push(&q_drop, (void *)13));
    assert(core_queue_ring_push(&q_drop, (void *)14));
    assert(core_queue_ring_pop(&q_drop, &out));
    assert(out == (void *)11);

    CoreQueueMutex mq;
    assert(core_queue_mutex_init(&mq, backing, 4));
    assert(!core_queue_mutex_timed_pop(&mq, &out, 1));
    assert(core_queue_mutex_push(&mq, (void *)42));
    assert(core_queue_mutex_timed_pop(&mq, &out, 10));
    assert(out == (void *)42);
    core_queue_mutex_destroy(&mq);

    return 0;
}
