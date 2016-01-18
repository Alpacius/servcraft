#include    "./p7impl.h"
#include    "./p7_root_alloc.h"
#include    "../include/model_alloc.h"
#include    "./p7_namespace.h"

// XXX WTF I've written. Get rid of these pieces of CRAP whenever possible.
// XXX UPDATE. These are still crap.

#define     atom_fetch_int32(_x_) \
({ \
    __atomic_load_n(&(_x_), __ATOMIC_SEQ_CST); \
})

#define     atom_store_int32(_d_, _x_) \
({ \
    __atomic_store_n(&(_d_), (_x_), __ATOMIC_SEQ_CST); \
})

#define     atom_add_uint32(_x_) \
({ \
    __atomic_add_fetch(&(_x_), 1, __ATOMIC_SEQ_CST); \
})

static int p7_timer_compare(const void *ev1, const void *ev2);

static struct p7_carrier **carriers = NULL;
static volatile uint32_t next_carrier = 0, ncarriers = 1;
static __thread struct p7_carrier *self_view = NULL;

static uint8_t active_queue_at[256] = { [0] = 0, [255] = 1 };

static
struct p7_coro_cntx *p7_coro_cntx_new_(void (*entry)(void *), void *arg, size_t stack_size, struct p7_limbo *limbo) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    struct p7_coro_cntx *cntx = scraft_allocate(allocator, (sizeof(struct p7_coro_cntx) + sizeof(void *) * stack_size + STACK_RESERVED_SIZE));
    if (cntx != NULL) {
        cntx->stack_size = stack_size;
        if (stack_size > 0)
            (cntx->uc.uc_stack.ss_sp = cntx->stack), (cntx->uc.uc_stack.ss_size = stack_size);
        cntx->uc.uc_link = (limbo != NULL) ? &(limbo->cntx->uc) : NULL;
        // TODO invalidate reserved area of coro's stack
        if (entry != NULL) {
            getcontext(&(cntx->uc));
            makecontext(&(cntx->uc), (void (*)()) entry, 1, arg);
        }
    }
    return cntx;
}

static
void p7_coro_cntx_delete_(struct p7_coro_cntx *cntx) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    scraft_deallocate(allocator, cntx);
}

static
struct p7_coro *p7_coro_new_(void (*entry)(void *), void *arg, size_t stack_size, unsigned carrier_id, struct p7_limbo *limbo) {
    struct p7_coro *coro = NULL;
    struct p7_coro_cntx *cntx = p7_coro_cntx_new_(entry, arg, stack_size, limbo);
    if (cntx != NULL) {
        __auto_type allocator = p7_root_alloc_get_proxy();
        coro = scraft_allocate(allocator, (sizeof(struct p7_coro)));
        if (coro != NULL) {
            (coro->carrier_id = carrier_id), (coro->cntx = cntx), (coro->following = NULL);
            (coro->func_info.entry = entry), (coro->func_info.arg = arg);
            coro->timedout = coro->resched = 0;
            coro->status = P7_CORO_STATUS_ALIVE;
            coro->trapper = NULL;
            (coro->mailbox_cleanup = NULL), (coro->mailbox_cleanup_arg = NULL);
            init_list_head(&(coro->mailbox));
        }
    }
    return coro;
}

static
void p7_coro_delete_(struct p7_coro *coro) {
    p7_coro_cntx_delete_(coro->cntx);
    __auto_type allocator = p7_root_alloc_get_proxy();
    scraft_deallocate(allocator, coro);
}

static
struct p7_coro_cntx *p7_coro_cntx_new(void (*entry)(void *), void *arg, size_t stack_size, struct p7_limbo *limbo) {
    return p7_coro_cntx_new_(entry, arg, stack_size, limbo);
}

static
struct p7_coro *p7_coro_new(void (*entry)(void *), void *arg, size_t stack_size, unsigned carrier_id, struct p7_limbo *limbo) {
    if (!list_is_empty(&(self_view->sched_info.coro_pool_tl))) {
        list_ctl_t *cororef = self_view->sched_info.coro_pool_tl.next;
        list_del(cororef);
        self_view->sched_info.coro_pool_size--;
        struct p7_coro *coro = container_of(cororef, struct p7_coro, lctl);
        (coro->func_info.arg = arg), (coro->func_info.entry = entry), (coro->carrier_id = carrier_id);
        if (stack_size <= coro->cntx->stack_size) {
            coro->cntx->uc.uc_stack.ss_sp = coro->cntx->stack;
            coro->cntx->uc.uc_link = (limbo != NULL) ? &(limbo->cntx->uc) : NULL;
            getcontext(&(coro->cntx->uc));
            makecontext(&(coro->cntx->uc), (void (*)()) entry, 1, arg);
        } else {
            p7_coro_cntx_delete_(coro->cntx);
            coro->cntx = p7_coro_cntx_new(entry, arg, stack_size, limbo);
        }
        coro->status = P7_CORO_STATUS_ALIVE;
        coro->trapper = NULL;
        coro->timedout = coro->resched = 0;
        (coro->mailbox_cleanup = NULL), (coro->mailbox_cleanup_arg = NULL);
        return coro;
    } else {
        return p7_coro_new_(entry, arg, stack_size, carrier_id, limbo);
    }
}

static
void p7_coro_delete(struct p7_coro *coro) {
    coro->status = P7_CORO_STATUS_DYING;
    coro->trapper = NULL;
    // TODO name cancellation
    if (self_view->sched_info.coro_pool_size < self_view->sched_info.coro_pool_cap) {
        self_view->sched_info.coro_pool_size++;
        coro->following = NULL;
        list_add_tail(&(coro->lctl), &(self_view->sched_info.coro_pool_tl));
    } else {
        p7_coro_delete_(coro);
    }
}

static
struct p7_limbo *p7_limbo_new(void (*entry)(void *)) {
    struct p7_coro_cntx *cntx = p7_coro_cntx_new(entry, NULL, 1024, NULL);
    struct p7_limbo *limbo = NULL;
    if (cntx != NULL) {
        __auto_type allocator = p7_root_alloc_get_proxy();
        limbo = scraft_allocate(allocator, sizeof(struct p7_limbo));
        if (limbo == NULL) {
            p7_coro_cntx_delete_(cntx);
            return NULL;
        }
        (limbo->entry = entry), (limbo->cntx = cntx);
    }
    return limbo;
}

static
struct p7_carrier *p7_carrier_prepare(unsigned carrier_id, unsigned nevents, void (*limbo_entry)(void *), void (*entry)(void *), void *arg) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    struct p7_carrier *carrier = scraft_allocate(allocator, sizeof(struct p7_carrier));
    struct epoll_event *evqueue = scraft_allocate(allocator, sizeof(struct epoll_event) * nevents);
    struct p7_limbo *limbo = p7_limbo_new(limbo_entry);
    void local_free(void *ptr) {
        scraft_deallocate(allocator, ptr);
    }
    if ( (carrier != NULL) && (evqueue != NULL) && (limbo != NULL) ) {
        carrier->carrier_id = carrier_id;
        carrier->sched_info.running = NULL;
        (carrier->startup.at_startup = NULL), (carrier->startup.arg_startup = NULL);
        carrier->sched_info.active_idx = P7_ACTIVE_IDX_MAGIC;
        init_list_head(&(carrier->sched_info.coro_queue));
        init_list_head(&(carrier->sched_info.blocking_queue));
        init_list_head(&(carrier->sched_info.dying_queue));
        init_list_head(&(carrier->sched_info.rq_pool_tl));
        init_list_head(&(carrier->sched_info.coro_pool_tl));
        init_list_head(&(carrier->sched_info.waitk_pool_tl));
        init_list_head(&(carrier->sched_info.rq_queues[0]));
        init_list_head(&(carrier->sched_info.rq_queues[1]));
        carrier->sched_info.rq_pool_size = carrier->sched_info.coro_pool_size = carrier->sched_info.waitk_pool_size = 0;
        (carrier->sched_info.rq_pool_cap = P7_RQ_POOL_CAP), (carrier->sched_info.coro_pool_cap = P7_CORO_POOL_CAP), (carrier->sched_info.waitk_pool_cap = P7_WAITK_POOL_CAP);
        carrier->sched_info.active_rq_queue = &(carrier->sched_info.rq_queues[0]);
        carrier->sched_info.local_rq_queue = &(carrier->sched_info.rq_queues[1]);
        pthread_spin_init(&(carrier->sched_info.mutex), PTHREAD_PROCESS_PRIVATE);
        pthread_spin_init(&(carrier->sched_info.rq_pool_lock), PTHREAD_PROCESS_PRIVATE);
        pthread_spin_init(&(carrier->sched_info.waitk_pool_lock), PTHREAD_PROCESS_PRIVATE);
        pthread_spin_init(&(carrier->sched_info.rq_queue_lock), PTHREAD_PROCESS_PRIVATE);
        scraft_rbt_init(&(carrier->sched_info.timer_queue), p7_timer_compare);
        (carrier->mgr_cntx.limbo = limbo), (carrier->mgr_cntx.sched = p7_coro_cntx_new(entry, arg, 1024 * 2, NULL)), 
        carrier->iomon_info.maxevents = nevents;
        init_list_head(&(carrier->iomon_info.queue));
        carrier->iomon_info.events = evqueue;
        carrier->iomon_info.epfd = epoll_create(nevents);
        carrier->iomon_info.is_blocking = 0;
        pipe(carrier->iomon_info.condpipe);
        int fdflags = fcntl(carrier->iomon_info.condpipe[0], F_GETFL, 0);
        fcntl(carrier->iomon_info.condpipe[0], F_SETFL, fdflags|O_NONBLOCK);
        carrier->icc_info.nmailboxes = ncarriers;
        uint32_t idx;
        carrier->icc_info.mailboxes = scraft_allocate(allocator, sizeof(struct p7_double_queue) * carrier->icc_info.nmailboxes);
        for (idx = 0; idx < carrier->icc_info.nmailboxes; idx++)
            p7_double_queue_init(&(carrier->icc_info.mailboxes[idx]));
        init_list_head(&(carrier->icc_info.localbox));
    } else {
#define free_safe(_p_) ({ do { if ((_p_) != NULL) local_free(_p_); } while (0); 0; })
        free_safe(carrier), free_safe(evqueue), free_safe(limbo);
    }
    if ( (carrier != NULL) && (carrier->iomon_info.epfd == -1) ) {
        free_safe(carrier), free_safe(evqueue), free_safe(limbo);
    }
#undef free_safe
    return carrier;
}

static 
void p7_carrier_atexit(struct p7_carrier *self) {
    // stub
    // TODO implementation
}

static
struct p7_coro_rq *p7_coro_rq_new_(void (*entry)(void *), void *arg, size_t stack_size) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    struct p7_coro_rq *rq = scraft_allocate(allocator, sizeof(struct p7_coro_rq));
    if (rq != NULL) {
        (rq->func_info.entry = entry), (rq->func_info.arg = arg);
        (rq->stack_info.stack_unit_size = sizeof(void *)), (rq->stack_info.stack_nunits = stack_size);
        rq->from = self_view->carrier_id;
    }
    return rq;
}

static
void p7_coro_rq_delete_(struct p7_coro_rq *rq) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    scraft_deallocate(allocator, rq);
}

static
struct p7_coro_rq *p7_coro_rq_new(void (*entry)(void *), void *arg, size_t stack_size) {
    pthread_spin_lock(&(self_view->sched_info.rq_pool_lock));
    if (!list_is_empty(&(self_view->sched_info.rq_pool_tl))) {
        list_ctl_t *rqref = self_view->sched_info.rq_pool_tl.next;
        list_del(rqref);
        self_view->sched_info.rq_pool_size--;
        pthread_spin_unlock(&(self_view->sched_info.rq_pool_lock));
        struct p7_coro_rq *rq = container_of(rqref, struct p7_coro_rq, lctl);
        (rq->stack_info.stack_unit_size = sizeof(void *)), (rq->stack_info.stack_nunits = stack_size);
        (rq->func_info.entry = entry), (rq->func_info.arg = arg);
        return rq;
    } else {
        pthread_spin_unlock(&(self_view->sched_info.rq_pool_lock));
        return p7_coro_rq_new_(entry, arg, stack_size);
    }
}

static
void p7_coro_rq_delete(struct p7_coro_rq *rq) {
    pthread_spin_lock(&(carriers[rq->from]->sched_info.rq_pool_lock));
    if (carriers[rq->from]->sched_info.rq_pool_size <= carriers[rq->from]->sched_info.rq_pool_cap) {
        carriers[rq->from]->sched_info.rq_pool_size++;
        list_add_tail(&(rq->lctl), &(carriers[rq->from]->sched_info.rq_pool_tl));
        pthread_spin_unlock(&(carriers[rq->from]->sched_info.rq_pool_lock));
    } else {
        pthread_spin_unlock(&(carriers[rq->from]->sched_info.rq_pool_lock));
        p7_coro_rq_delete_(rq);
    }
}

static
uint64_t get_timeval_current(void) {
    //struct timeval tv;
    //gettimeofday(&tv, NULL);
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME_COARSE, &tv);
    return ((uint64_t) tv.tv_sec * 1000) + ((uint64_t) tv.tv_nsec / 1000000);
}

static
uint64_t get_timeval_by_diff(uint64_t dt) {
    return get_timeval_current() + dt;
}

static
struct p7_timer_event *p7_timer_event_new_(uint64_t dt, unsigned from, struct p7_coro *coro, struct p7_cond_event *cond) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    struct p7_timer_event *ev = scraft_allocate(allocator, sizeof(struct p7_timer_event));
    if (ev != NULL)
        (ev->tval = get_timeval_by_diff(dt)), (ev->from = from), (ev->coro = coro), (ev->condref = cond), (ev->rbtctl.key_ref = &(ev->tval));
    return ev;
}

static
void p7_timer_event_init(struct p7_timer_event *ev, uint64_t dt, unsigned from, struct p7_coro *coro, struct p7_cond_event *cond) {
    (ev->tval = get_timeval_by_diff(dt)), (ev->from = from), (ev->coro = coro), (ev->condref = cond), (ev->rbtctl.key_ref = &(ev->tval));
}

static
void p7_timer_event_del_(struct p7_timer_event *ev) {
    if (ev->hook.dtor != NULL)
        ev->hook.dtor(ev->hook.arg, ev->hook.func);
    __auto_type allocator = p7_root_alloc_get_proxy();
    scraft_deallocate(allocator, ev);
}

static
struct p7_timer_event *p7_timer_event_new(uint64_t dt, unsigned from, struct p7_coro *coro, struct p7_cond_event *cond) {
    return p7_timer_event_new_(dt, from, coro, cond);
}

static
void p7_timer_event_del(struct p7_timer_event *ev) {
    p7_timer_event_del_(ev);
}

static
void p7_timer_event_hook(struct p7_timer_event *ev, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *))) {
    (ev->hook.arg = arg), (ev->hook.func = func), (ev->hook.dtor = dtor);
}

static
int p7_timer_compare(const void *lhs_, const void *rhs_) {
    uint64_t lhs = *((uint64_t *) lhs_), rhs = *((uint64_t *) rhs_);
    return (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);
}

static
void timer_add_event(struct p7_timer_event *ev, struct scraft_rbtree *queue) {
    scraft_rbt_insert(queue, &(ev->rbtctl));
}

static
void timer_remove_event(struct p7_timer_event *ev, struct scraft_rbtree *queue) {
    scraft_rbt_delete(queue, &(ev->rbtctl));
}

static
struct p7_timer_event *timer_peek_earliest(struct scraft_rbtree *queue) {
    struct scraft_rbtree_node *node = scraft_rbtree_min(queue);
    return (node != queue->sentinel) ? container_of(node, struct p7_timer_event, rbtctl) : NULL;
}

static
struct p7_timer_event *timer_extract_earliest(struct scraft_rbtree *queue) {
    struct scraft_rbtree_node *node = scraft_rbtree_min(queue);
    if (node != queue->sentinel) {
        scraft_rbt_delete(queue, node);
        return container_of(node, struct p7_timer_event, rbtctl);
    } else
        return NULL;
}

static
void limbo_loop(void *unused) {
    struct p7_carrier *self_carrier = self_view;
    while (1) {
        struct p7_coro *current = self_carrier->sched_info.running;
        __atomic_and_fetch(&(current->status), ~P7_CORO_STATUS_ALIVE, __ATOMIC_SEQ_CST);
        if (current->mailbox_cleanup != NULL)
            current->mailbox_cleanup(current->mailbox_cleanup_arg);
        list_del(&(current->lctl));
        if (current->following != NULL) {
            list_del(&(current->following->lctl));
            list_add_tail(&(current->following->lctl), &(self_carrier->sched_info.coro_queue));
        }
        self_carrier->sched_info.running = NULL;
        //p7_coro_delete(current);
        list_add_tail(&(current->lctl), &(self_carrier->sched_info.dying_queue));
        swapcontext(&(self_carrier->mgr_cntx.limbo->cntx->uc), &(self_carrier->mgr_cntx.sched->uc));
    }
}

static volatile uint32_t sched_loop_sync = 0;

static
void p7_intern_handle_wakeup(struct p7_intern_msg *message) {
    // XXX 'Tis empty
}

static
void p7_intern_handle_sent(struct p7_intern_msg *message) {
    struct p7_coro *coro = (struct p7_coro *) message->immpack_uintptr[0];
    if (__atomic_load_n(&(coro->status), __ATOMIC_SEQ_CST) & P7_CORO_STATUS_FLAG_RECV) {
        if (coro->resched == 0) {
            list_del(&(coro->lctl));
            list_add_tail(&(coro->lctl), &(self_view->sched_info.coro_queue));      // XXX 20160114: sched fix
            coro->resched = 1;
        }
        if (coro->timedout)
            coro->timedout = 0;
        __atomic_and_fetch(&(coro->status), ~P7_CORO_STATUS_FLAG_RECV, __ATOMIC_SEQ_CST);
    }
}

static
void *sched_loop(void *arg) {
    struct p7_carrier *self = (struct p7_carrier *) arg;
    self_view = self;
    list_ctl_t rq_backlog;
    init_list_head(&(rq_backlog));

    struct p7_waitk local_cond_k;
    local_cond_k.fd = self->iomon_info.condpipe[0];
    struct epoll_event local_cond_ev;
    (local_cond_ev.events = EPOLLIN), (local_cond_ev.data.ptr = &local_cond_k);
    epoll_ctl(self->iomon_info.epfd, EPOLL_CTL_ADD, self->iomon_info.condpipe[0], &local_cond_ev);

    int idx_active = 0;
#define     N_READ_VECTOR_MIN       4
#define     N_READ_VECTOR_MAX       32
    uint32_t n_read_vector = N_READ_VECTOR_MIN;

    // XXX sync
    atom_add_uint32(sched_loop_sync);
    while (__atomic_load_n(&sched_loop_sync, __ATOMIC_SEQ_CST) < ncarriers);

    if (self->startup.at_startup != NULL)
        self->startup.at_startup(self->startup.arg_startup);

    while (1) {
        pthread_spin_lock(&(self->sched_info.rq_queue_lock));
        int ep_timeout = (list_is_empty(&(self->sched_info.coro_queue)) && list_is_empty(&(self->sched_info.rq_queues[0])) && list_is_empty(&(self->sched_info.rq_queues[1]))) ? -1 : 0;
        atom_store_int32(self->iomon_info.is_blocking, 1);    // XXX slow but safe
        pthread_spin_unlock(&(self->sched_info.rq_queue_lock));
        struct p7_timer_event *ev_earliest = timer_peek_earliest(&(self->sched_info.timer_queue));
        uint64_t tval_before = get_timeval_current();
        if (ev_earliest != NULL) {
            if ((ev_earliest->tval > tval_before) && (ep_timeout < 0)) {
                ep_timeout = ev_earliest->tval - tval_before;
            }
        }
        int nactive = epoll_wait(self->iomon_info.epfd, self->iomon_info.events, self->iomon_info.maxevents, ep_timeout);
        atom_store_int32(self->iomon_info.is_blocking, 0);
        // XXX expire ALL available timers now. I know it's slow. 
        uint64_t tval_after = get_timeval_current();
        struct p7_timer_event *ev_timer_itr = NULL;
        while ((ev_timer_itr = timer_peek_earliest(&(self->sched_info.timer_queue))) != NULL) {
            if (ev_timer_itr->tval > tval_after)
                break;
            else {
                struct p7_timer_event *ev_timer_expired = ev_timer_itr;
                scraft_rbt_delete(&(self->sched_info.timer_queue), &(ev_timer_expired->rbtctl));
                if (ev_timer_expired->hook.func != NULL)
                    ev_timer_expired->hook.func(ev_timer_expired->hook.arg);
                if (ev_timer_expired->coro != NULL) {
                    ev_timer_expired->coro->timedout = 1;
                    if (ev_timer_expired->coro->resched == 0) {
                        list_del(&(ev_timer_expired->coro->lctl));
                        list_add_tail(&(ev_timer_expired->coro->lctl), &(self->sched_info.coro_queue));     // XXX 20160114: sched fix
                        ev_timer_expired->coro->resched = 1;
                    }
                }
                if (ev_timer_expired->condref != NULL) {
                    // TODO condref
                }
                //p7_timer_event_del(ev_timer_expired);
            }
        }
        int ep_itr;
        for (ep_itr = 0; ep_itr < nactive; ep_itr++) {
            struct p7_waitk *kwrap = (struct p7_waitk *) self->iomon_info.events[ep_itr].data.ptr;
            if (kwrap->fd != self->iomon_info.condpipe[0]) {
                // XXX be it slower when active connections are many.
                if (epoll_ctl(self->iomon_info.epfd, EPOLL_CTL_DEL, kwrap->fd, NULL) != -1) {
                    if (kwrap->coro->resched == 0) {
                        list_del(&(kwrap->coro->lctl));
                        list_add_tail(&(kwrap->coro->lctl), &(self->sched_info.coro_queue));    // XXX 20160114: sched fix
                        kwrap->coro->resched = 1;
                    }
                    if (kwrap->coro->timedout)
                        kwrap->coro->timedout = 0;
                    //p7_waitk_delete(kwrap);   XXX 20160103: removed persistent wait kontinuation
                }
            } else {
                void (*p7_intern_handlers[])(struct p7_intern_msg *) = {
                    NULL,   // XXX reserved0
                    p7_intern_handle_wakeup,
                    p7_intern_handle_sent,
                };
                uint32_t intern_handle_message(uint32_t vector_size) {
                    struct p7_intern_msg vector_message[vector_size];
                    struct iovec vector_io[vector_size];
                    uint32_t n_reading = 0, idx_vector;
                    for (idx_vector = 0; idx_vector < vector_size; idx_vector++)
                        (vector_io[idx_vector].iov_base = &(vector_message[idx_vector])), (vector_io[idx_vector].iov_len = sizeof(struct p7_intern_msg));
                    ssize_t size_read;
                    while ((size_read = readv(self->iomon_info.condpipe[0], vector_io, vector_size)) > 0) {
                        n_reading++;
                        uint32_t n_messages = size_read / sizeof(struct p7_intern_msg);     // XXX intended to be atomic
                        uint32_t idx;
                        for (idx = 0; idx < n_messages; idx++)
                            p7_intern_handlers[vector_message[idx].type](&(vector_message[idx]));
                    }
                    return n_reading;
                }
                uint32_t n_passes = intern_handle_message(n_read_vector);
                (n_read_vector < N_READ_VECTOR_MAX) && ((n_passes > 1) && (n_read_vector *= 2));
            }
        }

        list_ctl_t *p, *t, *h;

        h = &(self->sched_info.rq_queues[active_queue_at[__atomic_fetch_xor(&(self->sched_info.active_idx), (uint8_t) -1, __ATOMIC_SEQ_CST)]]);
        if (!list_is_empty(h)) {
            list_foreach_remove(p, h, t) {
                list_del(t);
                struct p7_coro_rq *rq = container_of(t, struct p7_coro_rq, lctl);
                struct p7_coro *coro = p7_coro_new(rq->func_info.entry, rq->func_info.arg, rq->stack_info.stack_nunits, self->carrier_id, self->mgr_cntx.limbo);
                list_add_tail(&(coro->lctl), &(self->sched_info.coro_queue));
                p7_coro_rq_delete(rq);
            }
        }

        // Mail dispatching is done before dying coros are dead. 
        void mailbox_dispatch(list_ctl_t *box) {
            list_ctl_t *p, *t;
            list_foreach_remove(p, box, t) {
                list_del(t);
                struct p7_msg *msg = container_of(t, struct p7_msg, lctl);
                struct p7_coro *dst = msg->dst;
                if (__atomic_load_n(&(dst->status), __ATOMIC_SEQ_CST) & P7_CORO_STATUS_ALIVE)
                    list_add_tail(&(msg->lctl), &(dst->mailbox));
                else if (msg->dtor != NULL)
                    msg->dtor(msg, msg->dtor_arg);
            }
        }
        uint32_t idx_mailbox;
        for (idx_mailbox = 0; idx_mailbox < self->icc_info.nmailboxes; idx_mailbox++) {
            uint8_t debug_idx;
            list_ctl_t *target_mailbox = &(self->icc_info.mailboxes[idx_mailbox].queue[debug_idx = active_queue_at[__atomic_fetch_xor(&(self->icc_info.mailboxes[idx_mailbox].active_idx), (uint8_t) -1, __ATOMIC_SEQ_CST)]]);
            mailbox_dispatch(target_mailbox);
        }
        mailbox_dispatch(&(self->icc_info.localbox));

        // Now we're safe. R.I.P. dying workers.
        h = &(self->sched_info.dying_queue);
        list_foreach_remove(p, h, t) {
            list_del(t);
            struct p7_coro *coro_dying = container_of(t, struct p7_coro, lctl);
            if ((__atomic_load_n(&(coro_dying->status), __ATOMIC_SEQ_CST) & P7_CORO_STATUS_FLAG_DECAY) == 0)
                p7_coro_delete(coro_dying);
        }

        // into the fight we leap
        // sched_info.running is only a "view"
        // XXX if we come back from limbo or iowrap, just pick the next coro
        if (!list_is_empty(&(self->sched_info.coro_queue))) {
            if (self->sched_info.running != NULL) {
                list_ctl_t *last_coro = &(self->sched_info.running->lctl); //self->sched_info.coro_queue.next;
                if (last_coro == self->sched_info.coro_queue.next) {
                    list_del(last_coro);
                    list_add_tail(last_coro, &(self->sched_info.coro_queue));
                }
            }
            list_ctl_t *next_coro = self->sched_info.coro_queue.next;
            self->sched_info.running = container_of(next_coro, struct p7_coro, lctl);
            self->sched_info.running->resched = 0;
            swapcontext(&(self->mgr_cntx.sched->uc), &(self->sched_info.running->cntx->uc));
        }
    }

    p7_carrier_atexit(self);
    return NULL;

#undef      N_READ_VECTOR_MIN
#undef      N_READ_VECTOR_MAX
}

// NOTE this wrapper is used for the main thread, which needs a scheduler but holds no independent pthread
static
void sched_loop_cntx_wraper(void *unused_arg) {
    sched_loop(carriers[0]);
}

void p7_coro_yield(void);

static
int coro_create_request(void (*entry)(void *), void *arg, size_t stack_size) {
    //atom_add_uint32(next_carrier);
    uint32_t next_carrier_id = __atomic_fetch_add(&next_carrier, 1, __ATOMIC_SEQ_CST);
    struct p7_carrier *next_load = carriers[next_carrier_id % ncarriers];
    if (next_load->carrier_id != self_view->carrier_id) {
        struct p7_coro_rq *rq = p7_coro_rq_new(entry, arg, stack_size);
        if (rq != NULL) {
            // TODO not-so-heavy performace loss
            pthread_spin_lock(&(next_load->sched_info.rq_queue_lock));
            list_add_tail(&(rq->lctl), &(next_load->sched_info.rq_queues[active_queue_at[__atomic_load_n(&(next_load->sched_info.active_idx), __ATOMIC_SEQ_CST)]]));
            pthread_spin_unlock(&(next_load->sched_info.rq_queue_lock));
            if (atom_fetch_int32(next_load->iomon_info.is_blocking)) {
                struct p7_intern_msg wakemsg = { .type = P7_INTERN_WAKEUP };
#define     P7_SEND_TIMES    2
                uint32_t nsendtimes = 0;
                while (write(next_load->iomon_info.condpipe[1], &wakemsg, sizeof(wakemsg)) == -1) {
                    if (++nsendtimes == P7_SEND_TIMES) {
                        nsendtimes = 0;
                        p7_coro_yield();
                    }
                }
#undef      P7_SEND_TIMES
            }
        } else
            return -1;
    } else {
        struct p7_coro *coro = p7_coro_new(entry, arg, stack_size, self_view->carrier_id, self_view->mgr_cntx.limbo);
        list_add_tail(&(coro->lctl), &(self_view->sched_info.coro_queue));
    }
    return (next_carrier_id % ncarriers) == self_view->carrier_id;
}

struct p7_carrier *p7_carrier_self_tl(void) {
    return self_view;
}

unsigned p7_get_ncarriers(void) {
    return ncarriers;
}

// APIs begin here

struct p7_timer_event *p7_timed_event(uint64_t dt, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *))) {
    struct p7_timer_event *ev = p7_timer_event_new_(dt, self_view->carrier_id, NULL, NULL);
    p7_timer_event_hook(ev, func, arg, dtor);
    timer_add_event(ev, &(self_view->sched_info.timer_queue));
    return ev;
}

struct p7_timer_event *p7_timed_event_assoc(uint64_t dt, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *))) {
    struct p7_timer_event *ev = p7_timer_event_new_(dt, self_view->carrier_id, self_view->sched_info.running, NULL);
    p7_timer_event_hook(ev, func, arg, dtor);
    timer_add_event(ev, &(self_view->sched_info.timer_queue));
    return ev;
}

struct p7_timer_event *p7_timed_event_immediate(struct p7_timer_event *ev, uint64_t dt, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *))) {
    ev->tval = get_timeval_by_diff(dt);
    ev->from = self_view->carrier_id;
    ev->coro = self_view->sched_info.running;
    ev->condref = NULL;
    ev->rbtctl.key_ref = &(ev->tval);
    p7_timer_event_hook(ev, func, arg, dtor);
    timer_add_event(ev, &(self_view->sched_info.timer_queue));
    return ev;
}

unsigned p7_timedout_(void) {
    return self_view->sched_info.running->timedout;
}

unsigned p7_timeout_reset(void) {
    self_view->sched_info.running->timedout = 0;
}

void p7_timer_clean__(struct p7_timer_event *ev) {
    timer_remove_event(ev, &(self_view->sched_info.timer_queue));
}

void p7_timer_clean_(struct p7_timer_event *ev) {
    timer_remove_event(ev, &(self_view->sched_info.timer_queue));
    p7_timer_event_del(ev);
}

void p7_timer_clean(struct p7_timer_event *ev) {
    p7_timer_event_del(ev);
}

void p7_coro_yield(void) {
    struct p7_carrier *self = self_view;
    struct p7_coro *self_coro = self->sched_info.running;
    swapcontext(&(self_coro->cntx->uc), &(self->mgr_cntx.sched->uc));
}

int p7_coro_create(void (*entry)(void *), void *arg, size_t stack_size) {
    if (coro_create_request(entry, arg, stack_size) > -1) {
        p7_coro_yield();
        return 0;
    } else
        return -1;
}

int p7_coro_create_async(void (*entry)(void *), void *arg, size_t stack_size) {
    return (coro_create_request(entry, arg, stack_size) > -1) ? 0 : -1;
}

int p7_coro_concat(void (*entry)(void *), void *arg, size_t stack_size) {
    struct p7_coro *coro = p7_coro_new(entry, arg, stack_size, self_view->carrier_id, self_view->mgr_cntx.limbo), *last = self_view->sched_info.running;
    if (coro == NULL)
        return -1;
    coro->following = last;
    list_del(&(last->lctl));
    list_add_tail(&(last->lctl), &(self_view->sched_info.blocking_queue));
    list_add_tail(&(coro->lctl), &(self_view->sched_info.coro_queue));
    self_view->sched_info.running = NULL;
    swapcontext(&(last->cntx->uc), &(self_view->mgr_cntx.sched->uc));
    return 0;
}

// XXX message sending and coro resched are independent in the scheduler
int p7_send_by_entity(void *dst, struct p7_msg *msg) {
    struct p7_coro *target = dst;
    struct p7_carrier *dst_carrier;
    msg->dst = dst;
    if ((target->status & P7_CORO_STATUS_ALIVE) == 0)
        return -1;
    if (target->carrier_id == self_view->carrier_id) {
        list_add_tail(&(msg->lctl), &(self_view->icc_info.localbox));
        dst_carrier = self_view;
    } else {
        struct p7_carrier *target_carrier = dst_carrier = carriers[target->carrier_id];
        list_ctl_t *q = &(target_carrier->icc_info.mailboxes[self_view->carrier_id].queue[active_queue_at[__atomic_load_n(&(target_carrier->icc_info.mailboxes[self_view->carrier_id].active_idx), __ATOMIC_SEQ_CST)]]);
        list_add_tail(&(msg->lctl), q);
    }
    struct p7_intern_msg msgbuf = { .type = P7_INTERN_SENT };
    msgbuf.immpack_uintptr[0] = (uintptr_t) target;
    msgbuf.immpack_uintptr[1] = (uintptr_t) self_view->sched_info.running;
#define P7_SEND_TIMES 4
    uint32_t nsendtimes = 0;
    while (write(dst_carrier->iomon_info.condpipe[1], &msgbuf, sizeof(msgbuf)) == -1) {
        if (++nsendtimes == P7_SEND_TIMES) {
            nsendtimes = 0;
            p7_coro_yield();
        }
    }
#undef  P7_SEND_TIMES
    return 0;
}

struct p7_msg *p7_recv(void) {
    list_ctl_t *ret;
    struct p7_coro *self = self_view->sched_info.running;
    if (list_is_empty(&(self->mailbox))) {
        //self->status |= P7_CORO_STATUS_FLAG_RECV;
        __atomic_or_fetch(&(self->status), P7_CORO_STATUS_FLAG_RECV, __ATOMIC_SEQ_CST);
        list_del(&(self->lctl));
        list_add_tail(&(self->lctl), &(self_view->sched_info.blocking_queue));
        self_view->sched_info.running = NULL;
        swapcontext(&(self->cntx->uc), &(self_view->mgr_cntx.sched->uc));
        __atomic_and_fetch(&(self->status), ~P7_CORO_STATUS_FLAG_RECV, __ATOMIC_SEQ_CST);
    } 
    if (!list_is_empty(&(self->mailbox))) {
        ret = self->mailbox.next;
        list_del(ret);
        return container_of(ret, struct p7_msg, lctl);
    } else 
        return NULL;
}

int p7_send_by_name(const char *name, struct p7_msg *msg) {
    struct p7_coro *dst = p7_namespace_find(name);
    if (dst != NULL) {
        __atomic_or_fetch(&(dst->status), P7_CORO_STATUS_FLAG_DECAY, __ATOMIC_SEQ_CST);
        int ret = p7_send_by_entity(dst, msg);
        __atomic_and_fetch(&(dst->status), ~P7_CORO_STATUS_FLAG_DECAY, __ATOMIC_SEQ_CST);
    } else
        return -1;
}

void *p7_coro_register_name(const char *name) {
    return p7_name_register(self_view->sched_info.running, name);
}

void p7_coro_discard_name(void *name_handle) {
    p7_name_discard(name_handle);
}

void p7_coro_set_mailbox_cleaner(void (*cleaner)(void *)) {
    self_view->sched_info.running->mailbox_cleanup = cleaner;
}

void p7_coro_set_mailbox_cleaner_arg(void *arg) {
    self_view->sched_info.running->mailbox_cleanup_arg = arg;
}

void *p7_coro_get_mailbox_cleaner_arg(void) {
    return self_view->sched_info.running->mailbox_cleanup_arg;
}

struct p7_msg *p7_mailbox_extract(void) {
    list_ctl_t *ret = NULL;
    if (!list_is_empty(&(self_view->sched_info.running->mailbox))) {
        ret = self_view->sched_info.running->mailbox.next;
        list_del(ret);
        return container_of(ret, struct p7_msg, lctl);
    } else 
        return NULL;
}

int p7_iowrap_(int fd, int rdwr) {
    struct p7_waitk k;
    k.from = self_view->carrier_id;
    k.coro = self_view->sched_info.running;
    k.fd = fd;
    (k.event.data.ptr = &k), (k.event.events = EPOLLONESHOT);
    ((rdwr & P7_IOMODE_READ) && (k.event.events |= EPOLLIN)), ((rdwr & P7_IOMODE_WRITE) && (k.event.events |= EPOLLOUT));
    (rdwr & P7_IOCTL_ET) && (k.event.events |= EPOLLET);
    (rdwr & P7_IOMODE_ERROR) && (k.event.events |= EPOLLERR); // useless
    int ret;
    ret = epoll_ctl(self_view->iomon_info.epfd, EPOLL_CTL_ADD, fd, &(k.event));
    if (ret == -1) {
        int errsv = errno;
        return -1;
    }
    list_del(&(k.coro->lctl));
    list_add_tail(&(k.coro->lctl), &(self_view->sched_info.blocking_queue));
    self_view->sched_info.running = NULL;
    swapcontext(&(k.coro->cntx->uc), &(self_view->mgr_cntx.sched->uc));
    return ret;
}

int p7_preinit_namespace_size(uint64_t namespace_size) {
#define P7_NAMESPACE_SIZE   512
    return p7_namespace_init((namespace_size) ? namespace_size : P7_NAMESPACE_SIZE);
#undef  P7_NAMESPACE_SIZE
}

static
int p7_init_real(unsigned nthreads, void (*at_startup)(void *), void *arg) {
    if (nthreads < 1)
        nthreads = 1;
    ncarriers = nthreads;
    __auto_type allocator = p7_root_alloc_get_proxy();
    carriers = scraft_allocate(allocator, sizeof(struct p7_carrier *) * ncarriers);
    int carrier_idx;
    carriers[0] = p7_carrier_prepare(0, 1024, limbo_loop, sched_loop_cntx_wraper, NULL);
    self_view = carriers[0];
    for (carrier_idx = 1; carrier_idx < ncarriers; carrier_idx++)
        carriers[carrier_idx] = p7_carrier_prepare(carrier_idx, 1024, limbo_loop, NULL, NULL);
    struct p7_coro *main_ctlflow = p7_coro_new(NULL, NULL, 0, 0, NULL);
    getcontext(&(main_ctlflow->cntx->uc));
    list_add_tail(&(main_ctlflow->lctl), &(carriers[0]->sched_info.coro_queue));
    carriers[0]->sched_info.running = main_ctlflow;
    if (at_startup != NULL)
        (carriers[0]->startup.at_startup = at_startup), (carriers[0]->startup.arg_startup = arg);
    for (carrier_idx = 1; carrier_idx < ncarriers; carrier_idx++) {
        if (at_startup != NULL)
            (carriers[carrier_idx]->startup.at_startup = at_startup), (carriers[carrier_idx]->startup.arg_startup = arg);
        pthread_create(&(carriers[carrier_idx]->tid), NULL, sched_loop, carriers[carrier_idx]);
    }
    swapcontext(&(main_ctlflow->cntx->uc), &(carriers[0]->mgr_cntx.sched->uc));
    return 0;
}

__asm__(".symver p7_init_0_1,p7_init@LIBP7_0.1");
int p7_init_0_1(unsigned nthreads, void (*at_startup)(void *), void *arg) {
    return p7_init_real(nthreads, at_startup, arg);
}

__asm__(".symver p7_init_0_4,p7_init@@LIBP7_0.4");
int p7_init_0_4(struct p7_init_config config) {
    return p7_preinit_namespace_size(config.namespace_config.namespace_size) 
        && p7_namespace_guard_init(config.namespace_config.rwlock_granularity, config.namespace_config.spintime) 
        && (p7_init_real(config.pthread_config.nthreads, config.pthread_config.at_startup, config.pthread_config.arg_startup) == 0);
}
