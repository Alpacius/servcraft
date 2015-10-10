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
#include    "../include/util_list.h"
#include    "util_heap.h"

#include    <assert.h>

struct p7_coro_cntx {
    list_ctl_t lctl;
    ucontext_t uc;
    unsigned stack_size;
    void *stack[];
};

struct p7_coro {
    list_ctl_t lctl;
    struct {
        void *arg;
        void (*entry)(void *);
    } func_info;
    unsigned carrier_id;
    unsigned timedout;
    struct p7_coro_cntx *cntx;
};

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
    struct p7_coro *coro;
    struct p7_cond_event *condref;
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

struct p7_carrier {
    pthread_t tid;
    unsigned carrier_id;
    struct {
        list_ctl_t coro_queue;
        list_ctl_t rq_pool_tl, coro_pool_tl, waitk_pool_tl;
        list_ctl_t rq_queues[2], *active_rq_queue, *local_rq_queue;
        pthread_spinlock_t mutex;
        pthread_spinlock_t rq_pool_lock, waitk_pool_lock;
        struct p7_coro *running;
        unsigned rq_pool_cap, rq_pool_size, coro_pool_cap, coro_pool_size, waitk_pool_cap, waitk_pool_size;
        struct p7_minheap *timer_heap;
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
        void (*at_startup)(void *);
        void *arg_startup;
    } startup;
};

#define     STACK_RESERVED_SIZE     (1024 * sizeof(void *))

#define     P7_IOMODE_READ      1
#define     P7_IOMODE_WRITE     2
#define     P7_IOMODE_ERROR     8
#define     P7_IOCTL_ET         4

#define     P7_RQ_POOL_CAP      128
#define     P7_CORO_POOL_CAP    64
#define     P7_WAITK_POOL_CAP   128

#define     P7_NTIMERS_INIT     128

#endif      // _P7_IMPL_H_
