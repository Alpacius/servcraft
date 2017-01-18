#ifndef     P7R_CPBUFFER_H_
#define     P7R_CPBUFFER_H_

// Comes from cleanwater.

#include    "./p7r_stdc_common.h"
#include    "../include/util_list.h"

struct p7r_cpbuffer {
    uint8_t index_producer;
    uint8_t consuming;
    list_ctl_t buffers[2];
};

#define     CP_BUFFER_CONSUMING                  0
#define     CP_BUFFER_FREE                       1      // XXX Do NOT check this constant

#define     CP_BUFFER_PRODUCER_BUSY              0x2
#define     CP_BUFFER_INITIAL_PRODUCER_INDEX     0x0

#define     cp_buffer_normalized_index(index_)   ((index_) & 0x1)
#define     cp_buffer_flipped_index(index_)      (1 - cp_buffer_normalized_index((index_)))
#define     cp_buffer_flipped_index_raw(index_)  (1 - (index_))
#define     cp_buffer_consumer_index(queue_)     (1 - cp_buffer_normalized_index((queue_)->index_producer))

#define     cp_buffer_index_of(queue_)           __atomic_load_n(&((queue_)->index_producer), __ATOMIC_ACQUIRE)

// Beware: the consumer may fail to refresh its buffer.
static inline
list_ctl_t *cp_buffer_consume(struct p7r_cpbuffer *queue) {
    if (list_is_empty(&(queue->buffers[cp_buffer_consumer_index(queue)]))) {
        uint8_t desired_index = cp_buffer_normalized_index(cp_buffer_index_of(queue));
        queue->consuming = __atomic_compare_exchange_n(&(queue->index_producer), &desired_index, cp_buffer_flipped_index_raw(desired_index), 0, __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
        return &(queue->buffers[cp_buffer_flipped_index(queue->index_producer)]);
    }
    return &(queue->buffers[cp_buffer_consumer_index(queue)]);
}

static inline
void cp_buffer_produce(struct p7r_cpbuffer *queue, list_ctl_t *product) {
    __atomic_or_fetch(&(queue->index_producer), CP_BUFFER_PRODUCER_BUSY, __ATOMIC_RELEASE);
    __atomic_thread_fence(__ATOMIC_ACQ_REL);
    {
        list_ctl_t *target_queue = &(queue->buffers[cp_buffer_normalized_index(cp_buffer_index_of(queue))]);
        list_add_tail(product, target_queue);
    }
    __atomic_and_fetch(&(queue->index_producer), ~CP_BUFFER_PRODUCER_BUSY, __ATOMIC_RELEASE);
}

static inline
struct p7r_cpbuffer *cp_buffer_init(struct p7r_cpbuffer *queue) {
    queue->consuming = CP_BUFFER_FREE;
    __atomic_store_n(&(queue->index_producer), CP_BUFFER_INITIAL_PRODUCER_INDEX, __ATOMIC_RELEASE);
    init_list_head(&(queue->buffers[0]));
    init_list_head(&(queue->buffers[1]));
}

#undef      CP_BUFFER_PRODUCER_BUSY
#undef      CP_BUFFER_INITIAL_PRODUCER_INDEX

#undef      cp_buffer_normalized_index
#undef      cp_buffer_flipped_index
#undef      cp_buffer_flipped_index_raw
#undef      cp_buffer_consumer_index

#undef      cp_buffer_index_of

#endif      // P7R_CPBUFFER_H_
