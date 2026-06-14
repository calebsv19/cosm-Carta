#include "core_queue.h"

#include <assert.h>

int main(void) {
    void *backing[4] = {0};
    CoreQueueRing q;
    void *out = 0;

    assert(!core_queue_ring_init(NULL, backing, 4));
    assert(!core_queue_ring_init(&q, NULL, 4));
    assert(!core_queue_ring_init(&q, backing, 0));
    assert(!core_queue_ring_init_ex(&q, backing, 4, (CoreQueueOverflowPolicy)99));

    assert(core_queue_ring_init(&q, backing, 4));
    assert(core_queue_ring_size(NULL) == 0);
    {
        CoreQueueStats empty = core_queue_ring_stats(NULL);
        assert(empty.push_ok == 0);
        assert(empty.push_fail == 0);
        assert(empty.pop_ok == 0);
        assert(empty.pop_empty == 0);
        assert(empty.drops == 0);
    }
    assert(core_queue_ring_push(&q, (void *)1));
    assert(core_queue_ring_push(&q, (void *)2));

    assert(!core_queue_ring_pop(NULL, &out));
    assert(!core_queue_ring_pop(&q, NULL));
    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)1);

    assert(core_queue_ring_push(&q, (void *)3));
    assert(core_queue_ring_push(&q, (void *)4));
    assert(core_queue_ring_push(&q, (void *)5));
    assert(!core_queue_ring_push(&q, (void *)6));
    assert(core_queue_ring_size(&q) == 4);

    CoreQueueStats rs = core_queue_ring_stats(&q);
    assert(rs.push_ok == 5);
    assert(rs.push_fail == 1);
    assert(rs.pop_ok == 1);
    assert(rs.pop_empty == 0);
    assert(rs.drops == 0);

    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)2);
    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)3);
    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)4);
    assert(core_queue_ring_pop(&q, &out));
    assert(out == (void *)5);
    assert(!core_queue_ring_pop(&q, &out));
    rs = core_queue_ring_stats(&q);
    assert(rs.pop_empty == 1);

    CoreQueueRing q_drop;
    assert(!core_queue_ring_init_ex(NULL, backing, 4, CORE_QUEUE_OVERFLOW_DROP_OLDEST));
    assert(core_queue_ring_init_ex(&q_drop, backing, 4, CORE_QUEUE_OVERFLOW_DROP_OLDEST));
    assert(core_queue_ring_push(&q_drop, (void *)10));
    assert(core_queue_ring_push(&q_drop, (void *)11));
    assert(core_queue_ring_push(&q_drop, (void *)12));
    assert(core_queue_ring_push(&q_drop, (void *)13));
    assert(core_queue_ring_push(&q_drop, (void *)14));
    assert(core_queue_ring_pop(&q_drop, &out));
    assert(out == (void *)11);
    rs = core_queue_ring_stats(&q_drop);
    assert(rs.push_ok == 5);
    assert(rs.push_fail == 0);
    assert(rs.pop_ok == 1);
    assert(rs.pop_empty == 0);
    assert(rs.drops == 1);

    CoreQueueMutex mq;
    assert(!core_queue_mutex_init(NULL, backing, 4));
    assert(!core_queue_mutex_init_ex(&mq, backing, 4, (CoreQueueOverflowPolicy)99));
    assert(core_queue_mutex_init(&mq, backing, 4));
    assert(core_queue_mutex_size(NULL) == 0);
    {
        CoreQueueStats empty = core_queue_mutex_stats(NULL);
        assert(empty.push_ok == 0);
        assert(empty.push_fail == 0);
        assert(empty.pop_ok == 0);
        assert(empty.pop_empty == 0);
        assert(empty.drops == 0);
    }
    assert(!core_queue_mutex_push(NULL, (void *)1));
    assert(!core_queue_mutex_pop(NULL, &out));
    assert(!core_queue_mutex_timed_pop(NULL, &out, 1));
    assert(!core_queue_mutex_pop(&mq, NULL));
    assert(!core_queue_mutex_timed_pop(&mq, NULL, 1));
    assert(!core_queue_mutex_timed_pop(&mq, &out, 1));
    assert(core_queue_mutex_push(&mq, (void *)42));
    assert(core_queue_mutex_timed_pop(&mq, &out, 10));
    assert(out == (void *)42);
    assert(core_queue_mutex_size(&mq) == 0);
    rs = core_queue_mutex_stats(&mq);
    assert(rs.push_ok == 1);
    assert(rs.push_fail == 0);
    assert(rs.pop_ok == 1);
    assert(rs.pop_empty == 1);
    assert(rs.drops == 0);
    core_queue_mutex_destroy(&mq);

    return 0;
}
