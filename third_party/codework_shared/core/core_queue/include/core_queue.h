#ifndef CORE_QUEUE_H
#define CORE_QUEUE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <pthread.h>

typedef enum CoreQueueOverflowPolicy {
    CORE_QUEUE_OVERFLOW_REJECT = 0,
    CORE_QUEUE_OVERFLOW_DROP_OLDEST = 1
} CoreQueueOverflowPolicy;

typedef struct CoreQueueStats {
    uint64_t push_ok;
    uint64_t push_fail;
    uint64_t pop_ok;
    uint64_t pop_empty;
    uint64_t drops;
} CoreQueueStats;

typedef struct CoreQueueRing {
    void **items;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    CoreQueueOverflowPolicy overflow_policy;
    CoreQueueStats stats;
} CoreQueueRing;

bool core_queue_ring_init(CoreQueueRing *q, void **backing, size_t capacity);
bool core_queue_ring_init_ex(
    CoreQueueRing *q,
    void **backing,
    size_t capacity,
    CoreQueueOverflowPolicy overflow_policy);
bool core_queue_ring_push(CoreQueueRing *q, void *item);
bool core_queue_ring_pop(CoreQueueRing *q, void **out_item);
size_t core_queue_ring_size(const CoreQueueRing *q);
CoreQueueStats core_queue_ring_stats(const CoreQueueRing *q);

/* Mutex + cond queue suitable for MPSC producer/consumer paths. */
typedef struct CoreQueueMutex {
    CoreQueueRing ring;
    pthread_mutex_t mu;
    pthread_cond_t cv;
} CoreQueueMutex;

bool core_queue_mutex_init(CoreQueueMutex *q, void **backing, size_t capacity);
bool core_queue_mutex_init_ex(
    CoreQueueMutex *q,
    void **backing,
    size_t capacity,
    CoreQueueOverflowPolicy overflow_policy);
void core_queue_mutex_destroy(CoreQueueMutex *q);
bool core_queue_mutex_push(CoreQueueMutex *q, void *item);
bool core_queue_mutex_pop(CoreQueueMutex *q, void **out_item);
bool core_queue_mutex_timed_pop(CoreQueueMutex *q, void **out_item, uint32_t timeout_ms);
size_t core_queue_mutex_size(CoreQueueMutex *q);
CoreQueueStats core_queue_mutex_stats(CoreQueueMutex *q);

#endif
