#ifndef     P7_IMPL_H_
#define     P7_IMPL_H_

#include    <stdlib.h>
#include    <string.h>
#include    <stdint.h>
#include    <unistd.h>
#include    <errno.h>
#include    <fcntl.h>
#include    <pthread.h>
#include    <ucontext.h>
#include    <time.h>
#include    <sys/epoll.h>
#include    <sys/types.h>
#include    <sys/uio.h>
#include    "../include/util_list.h"
#include    "../util/scraft_rbt_ifce.h"

#include    <assert.h>

struct p7_coro_cntx {
    list_ctl_t lctl;
    ucontext_t uc;
    unsigned stack_size;
    void *stack[] __attribute__((aligned(16)));
};

struct p7_coro {
    list_ctl_t lctl, mailbox;
    struct {
        unsigned stack_size;
    } cntx_info;
    struct {
        void *arg;
        void (*entry)(void *);
    } func_info;
    unsigned carrier_id;
    unsigned timedout, resched;
    uint32_t status;
    uint64_t decay;
    struct p7_coro_cntx *cntx;
    struct p7_coro *following, *trapper;
    void (*mailbox_cleanup)(void *);
    void *mailbox_cleanup_arg;
    int fd_waiting;
    struct {
        void *arg;
        void (*cleanup)(void *, void *);
    } cleanup_info;
};

#define     P7_CORO_STATUS_DYING            0
#define     P7_CORO_STATUS_ALIVE            1

#define     P7_CORO_STATUS_FLAG_SPAWNING    8
#define     P7_CORO_STATUS_FLAG_RECV        16
#define     P7_CORO_STATUS_FLAG_DECAY       32
#define     P7_CORO_STATUS_IOREADY          64
#define     P7_CORO_STATUS_FLAG_MAIN        128


// XXX I see frag!
struct p7_coro_rq {
    list_ctl_t lctl;
    struct {
        void *arg;
        void (*entry)(void *);
    } func_info;
    struct {
        size_t stack_unit_size;
        size_t stack_nunits;
    } stack_info;
    unsigned from;
};

struct p7_limbo {
    void (*entry)(void *);
    struct p7_coro_cntx *cntx;
};

struct p7_waitk {
    int fd;
    unsigned from;
    struct p7_coro *coro;
    struct epoll_event event;
    list_ctl_t lctl;
};

struct p7_timer_event;
struct p7_cond_event;

// NOTE a coroutine with a timer/cond event will NOT exit until it returns from the blocking invocation.
// NOTE check coro ?= NULL first.

// NOTE timer events are restricted to the local carrier.
struct p7_timer_event {
    uint64_t tval;
    unsigned from;
    int triggered;
    struct p7_coro *coro;
    struct p7_cond_event *condref;
    struct scraft_rbtree_node rbtctl;
    struct {
        void *arg;
        void (*func)(void *);
        void (*dtor)(void *, void (*)(void *));
    } hook;
};

// NOTE cond events are restricted to the local carrier.
struct p7_cond_event {
    char key[48];   // XXX 20140426 I'm at a doujin expo thus don't know if a length of 48B is enough.
    unsigned from;
    struct p7_coro *coro;
    struct p7_timer_event *timerref;
};

struct p7_double_queue {
    list_ctl_t queue[2];
    volatile uint8_t active_idx;
};

static inline
void p7_double_queue_init(struct p7_double_queue *q) {
    init_list_head(&(q->queue[0]));
    init_list_head(&(q->queue[1]));
    __atomic_store_n(&(q->active_idx), 0, __ATOMIC_SEQ_CST);
}

struct p7_carrier {
    pthread_t tid;
    unsigned carrier_id;
    int *alive;
    struct {
        list_ctl_t coro_queue, blocking_queue, dying_queue;
        list_ctl_t rq_pool_tl, coro_pool_tl, waitk_pool_tl;
        list_ctl_t rq_queues[2], *active_rq_queue, *local_rq_queue;
        volatile uint8_t active_idx;
        pthread_spinlock_t mutex;
        pthread_spinlock_t rq_queue_lock;
        pthread_spinlock_t rq_pool_lock, waitk_pool_lock;
        struct p7_coro *running;
        unsigned rq_pool_cap, rq_pool_size, coro_pool_cap, coro_pool_size, waitk_pool_cap, waitk_pool_size;
        struct scraft_rbtree timer_queue;
    } sched_info;
    struct {
        struct p7_limbo *limbo;
        struct p7_coro_cntx *sched;
    } mgr_cntx;
    struct {
        int epfd, maxevents;
        int condpipe[2], is_blocking;
        struct epoll_event *events;
        list_ctl_t queue;
    } iomon_info;
    struct {
        struct p7_double_queue *mailboxes;
        uint32_t nmailboxes;
        list_ctl_t localbox;
    } icc_info;
    struct {
        void (*at_startup)(void *);
        void *arg_startup;
    } startup;
};

struct p7_intern_msg {
    uint32_t type;
    uintptr_t immpack_uintptr[2];
    uint64_t immpack_uint64[2];
};

struct p7_msg {
    list_ctl_t lctl;
    void *dst;
    void (*dtor)(struct p7_msg *, void *);
    void *dtor_arg;
};

#define     P7_INTERN_RESERVED0 0
#define     P7_INTERN_WAKEUP    1
#define     P7_INTERN_SENT      2

#define     STACK_RESERVED_SIZE     (1024 * sizeof(void *))

#define     P7_IOMODE_READ      1
#define     P7_IOMODE_WRITE     2
#define     P7_IOMODE_ERROR     8
#define     P7_IOCTL_ET         4

#define     P7_RQ_POOL_CAP      128
#define     P7_CORO_POOL_CAP    64
#define     P7_WAITK_POOL_CAP   128

#define     P7_NTIMERS_INIT     128

#define     P7_ACTIVE_IDX_MAGIC 0

#define     P7_IO_NOTIFY_ERROR      -1
#define     P7_IO_NOTIFY_BASE        0
#define     P7_IO_NOTIFY_FDRDY       1
#define     P7_IO_NOTIFY_RCVRDY      2
#define     P7_IO_NOTIFY_RESCHED     4

struct p7_pthread_config {
    unsigned nthreads;
    void (*at_startup)(void *);
    void *arg_startup;
};

struct p7_namespace_config {
    uint64_t namespace_size;
    uint64_t rwlock_granularity;
    uint32_t spintime;
};

struct p7_init_config {
    struct p7_pthread_config pthread_config;
    struct p7_namespace_config namespace_config;
};

#endif      // P7_IMPL_H_
