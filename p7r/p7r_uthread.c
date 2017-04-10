#include    "./p7r_uthread.h"
#include    "./p7r_root_alloc.h"
#include    "./p7r_timing.h"


#define p7r_uthread_reenable(scheduler_, uthread_) \
    do { \
        __auto_type uthread__ = (uthread_); \
        if (uthread__->status != P7R_UTHREAD_RUNNING) { \
            p7r_uthread_detach(uthread__); \
            p7r_uthread_attach(uthread__, &((scheduler_)->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING])); \
            p7r_uthread_change_state_clean(uthread__, P7R_UTHREAD_RUNNING); \
        } \
    } while (0)


// globals

static struct p7r_scheduler *schedulers;
static struct p7r_carrier *carriers;
static pthread_barrier_t carrier_barrier;
static __thread struct p7r_carrier *self_carrier;
static struct p7r_uthread main_uthread = { .scheduler_index = 0, .status = P7R_UTHREAD_RUNNING };
static uint32_t balance_index = 0;

#define next_balance_index __atomic_add_fetch(&balance_index, 1, __ATOMIC_ACQ_REL)


// timers

static
struct p7r_timer_core *p7r_timer_core_init(struct p7r_timer_core *timer, uint64_t timestamp, struct p7r_uthread *uthread) {
    timer->triggered = 0;
    timer->uthread = uthread;
    timer->timestamp = timestamp;
    timer->maplink.key_ref = &(timer->timestamp);
    timer->maplink.meta = NULL;
    return timer;
}

static
struct p7r_timer_core *p7r_timer_core_init_diff(struct p7r_timer_core *timer, uint64_t diff, struct p7r_uthread *uthread) {
    return p7r_timer_core_init(timer, get_timestamp_ms_by_diff(diff), uthread);
}

static
void p7r_timer_core_attach(struct p7r_timer_queue *queue, struct p7r_timer_core *timer) {
    scraft_rbt_insert(&(queue->map), &(timer->maplink));
}

static
void p7r_timer_core_detach(struct p7r_timer_core *timer) {
    scraft_rbt_detach(&(timer->maplink));
}

static
int p7r_timer_core_compare(const void *lhs_, const void *rhs_) {
    uint64_t lhs = *((const uint64_t *) lhs_), rhs = *((const uint64_t *) rhs_);
    return (lhs == rhs) ? 0 : ( (lhs < rhs) ? -1 : 1 );
}

static
void p7r_timer_queue_init(struct p7r_timer_queue *queue) {
    scraft_rbt_init(&(queue->map), p7r_timer_core_compare);
}

static
struct p7r_timer_core *p7r_timer_peek_earliest(struct p7r_timer_queue *queue) {
    struct scraft_rbtree_node *node = scraft_rbtree_min(&(queue->map));
    return (node != queue->map.sentinel) ? container_of(node, struct p7r_timer_core, maplink) : NULL;
}


// uthreads & schedulers

static int sched_bus_refresh(struct p7r_scheduler *scheduler);
static struct p7r_uthread *sched_resched_target(struct p7r_scheduler *scheduler);
static struct p7r_uthread_request sched_cherry_pick(struct p7r_scheduler *scheduler);

static void sched_idle(struct p7r_uthread *uthread);

static void p7r_internal_message_delete(struct p7r_internal_message *message);

static inline
struct p7r_uthread_request *p7r_uthread_request_init(
        struct p7r_uthread_request *request,
        void (*entrance)(void *),
        void *argument) {
    return (request->user_entrance = entrance), (request->user_argument = argument), request;
}

static inline
struct p7r_uthread_request *p7r_uthread_request_new(void (*entrance)(void *), void *argument) {
    struct p7r_uthread_request *request = NULL;
    {
        __auto_type allocator = p7r_root_alloc_get_proxy();
        request = scraft_allocate(allocator, sizeof(struct p7r_uthread_request));
        (request) && ((request->user_entrance = entrance), (request->user_argument = argument));
    }
    return request;
}

static inline
void p7r_uthread_request_delete(struct p7r_uthread_request *request) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    scraft_deallocate(allocator, request);
}

static inline
void p7r_uthread_change_state_clean(struct p7r_uthread *uthread, uint64_t status) {
    __atomic_store_n(&(uthread->status), status, __ATOMIC_RELEASE);
}

static inline
void p7r_uthread_switch(struct p7r_uthread *to, struct p7r_uthread *from) {
    p7r_context_switch(&(to->context), &(from->context));
}

static inline
void p7r_uthread_detach(struct p7r_uthread *uthread) {
    list_del(&(uthread->linkable));
}

static inline
void p7r_uthread_attach(struct p7r_uthread *uthread, list_ctl_t *target) {
    list_add_tail(&(uthread->linkable), target);
}

static
void p7r_uthread_lifespan(void *uthread_) {
    struct p7r_uthread *self = uthread_;

    struct p7r_uthread_request reincarnation;
    struct p7r_scheduler *self_scheduler = &(schedulers[self->scheduler_index]);
    do {
        p7r_uthread_change_state_clean(self, P7R_UTHREAD_RUNNING);
        self->entrance.user_entrance(self->entrance.user_argument);
        p7r_uthread_change_state_clean(self, P7R_UTHREAD_LIMBO);
        reincarnation = sched_cherry_pick(self_scheduler);
        if (reincarnation.user_entrance) {
            (self->entrance.user_entrance = reincarnation.user_entrance), (self->entrance.user_argument = reincarnation.user_argument);
            {
                sched_bus_refresh(self_scheduler);
                struct p7r_uthread *next_balance = sched_resched_target(self_scheduler);
                p7r_uthread_switch(next_balance, self);
            }
        }
    } while (reincarnation.user_entrance);

    p7r_uthread_detach(self);
    p7r_uthread_change_state_clean(self, P7R_UTHREAD_DYING);
    list_add_tail(&(self->linkable), &(schedulers[self->scheduler_index].runners.sched_queues[P7R_SCHED_QUEUE_DYING]));
    schedulers[self->scheduler_index].runners.running = NULL;

    // Actually we never return, but that's one of things we would not tell the compiler
    p7r_context_switch(schedulers[self->scheduler_index].runners.carrier_context, &(self->context));
}

static
struct p7r_uthread *p7r_uthread_init(
        struct p7r_uthread *uthread, 
        uint32_t scheduler_index, 
        void (*user_entrance)(void *), 
        void *user_argument, 
        struct p7r_stack_metamark *stack_metamark) {
    uthread->scheduler_index = scheduler_index;
    uthread->stack_metamark = stack_metamark;
    (uthread->entrance.user_entrance = user_entrance), (uthread->entrance.user_argument = user_argument);
    (uthread->entrance.real_entrance = p7r_uthread_lifespan), (uthread->entrance.real_argument = uthread);
    p7r_context_init(&(uthread->context), stack_base_of(stack_metamark), stack_size_of(stack_metamark));
    p7r_context_prepare(&(uthread->context), uthread->entrance.real_entrance, uthread->entrance.real_argument);
    return uthread;
}

static inline
struct p7r_uthread *p7r_uthread_ruin(struct p7r_uthread *uthread) {
    // XXX empty implementation
    return uthread;
}

static
struct p7r_uthread *p7r_uthread_new(
        uint32_t scheduler_index, 
        void (*user_entrance)(void *), 
        void *user_argument, 
        struct p7r_stack_allocator *allocator, 
        uint8_t stack_alloc_policy) {
    struct p7r_stack_metamark *stack_meta = stack_metamark_create(allocator, stack_alloc_policy);
    if (unlikely(stack_meta == NULL)) 
        return NULL;
    struct p7r_uthread *uthread = (struct p7r_uthread *) stack_meta_of(stack_meta);
    return p7r_uthread_init(uthread, scheduler_index, user_entrance, user_argument, stack_meta);
}

static inline
void p7r_uthread_delete(struct p7r_uthread *uthread) {
    struct p7r_stack_metamark *stack_meta = p7r_uthread_ruin(uthread)->stack_metamark;
    stack_metamark_destroy(stack_meta);
}

static
void u2cc_handler_uthread_request(struct p7r_scheduler *scheduler, struct p7r_internal_message *message) {
    struct p7r_uthread_request *request = (struct p7r_uthread_request *) &(message->content_buffer);
    list_add_tail(&(request->linkable), &(scheduler->runners.request_queue));
}

static
void (*p7r_internal_handlers[])(struct p7r_scheduler *, struct p7r_internal_message *) = {
    [1] = u2cc_handler_uthread_request,
};

static
int sched_bus_refresh(struct p7r_scheduler *scheduler) {
    // Phase 1 - adjust timeout baseline
    int timeout = 0;
    if (scheduler->bus.consumed) {
        uint64_t current_time_before = get_timestamp_ms_current();
        struct p7r_timer_core *timer_earliest = p7r_timer_peek_earliest(&(scheduler->bus.timers));
        timeout = timer_earliest ? (timer_earliest->timestamp - current_time_before) : -1;
        ((timer_earliest) && (timeout < 1)) && (timeout = 0);
    }

    (!list_is_empty(&(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]))) && (timeout = 0);

    int n_active_fds = epoll_wait(scheduler->bus.fd_epoll, scheduler->bus.epoll_events, scheduler->bus.n_epoll_events, timeout);
    if (n_active_fds < 0)
        return -1;
    scheduler->bus.consumed = 1;        // XXX consumer flag must be reset here

    // Phase 2 - timeout handling
    uint64_t current_time = get_timestamp_ms_current();
    struct p7r_timer_core *timer_iterator = NULL;
    while (
            ((timer_iterator = p7r_timer_peek_earliest(&(scheduler->bus.timers))) != NULL) &&
            (timer_iterator->timestamp <= current_time)
          ) {
        scraft_rbt_detach(&(timer_iterator->maplink));
        timer_iterator->triggered = 1;
        p7r_uthread_reenable(scheduler, timer_iterator->uthread);
    }

    // Phase 3 - respond delegation events: i/o notification & internal wakeup
    for (int event_index = 0; event_index < n_active_fds; event_index++) {
        struct p7r_delegation *delegation = scheduler->bus.epoll_events[event_index].data.ptr;
        if (delegation == &(scheduler->bus.notification)) {
            uint64_t notification_counter;
            read(scheduler->bus.fd_notification, &notification_counter, sizeof(uint64_t));
        } else {
            delegation->checked_events.io.triggered = 1;
            // remove triggered timer event
            if (!delegation->checked_events.timer.triggered) {
                p7r_uthread_reenable(scheduler, delegation->uthread);
                scraft_rbt_detach(&(delegation->checked_events.timer.measurement.maplink));
            }
        }
    }

    // Phase 4 - iuc/u2cc handling
    for (uint32_t carrier_index = 0; carrier_index < scheduler->n_carriers; carrier_index++) {
        if (carrier_index != scheduler->index) {
            list_ctl_t *target_queue = cp_buffer_consume(&(scheduler->bus.message_boxes[carrier_index]));
            scheduler->bus.consumed &= scheduler->bus.message_boxes[carrier_index].consuming;
            list_ctl_t *p, *t;
            list_foreach_remove(p, target_queue, t) {
                list_del(t);
                struct p7r_internal_message *message = container_of(t, struct p7r_internal_message, linkable);
                p7r_internal_handlers[P7R_MESSAGE_REAL_TYPE(message->type)](scheduler, message);    // XXX highly dangerous
            }
        }
    }

    // Phase 5 - iuc discarding
    // TODO implementation

    // Phase 6 - R.I.P. those who chose not to reincarnate
    {
        list_ctl_t *p, *t;
        list_foreach_remove(p, &(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_DYING]), t) {
            list_del(t);
            struct p7r_uthread *uthread_dying = container_of(t, struct p7r_uthread, linkable);
            p7r_uthread_delete(uthread_dying);
        }
    }

    return 0;
}

static
struct p7r_uthread_request sched_cherry_pick(struct p7r_scheduler *scheduler) {
    struct p7r_uthread_request request = { .user_entrance = NULL, .user_argument = NULL };
    if (!list_is_empty(&(scheduler->runners.request_queue))) {
        list_ctl_t *target_link = scheduler->runners.request_queue.next;
        list_del(target_link);
        struct p7r_uthread_request *target_request = container_of(target_link, struct p7r_uthread_request, linkable);
        request = *target_request;
        struct p7r_internal_message *message = P7R_MESSAGE_OF(target_request);  // XXX u2cc message deletion
        p7r_internal_message_delete(message);
    }
    return request;
}

static
struct p7r_uthread *sched_uthread_from_request(
        struct p7r_scheduler *scheduler, 
        struct p7r_uthread_request request, 
        uint8_t stack_alloc_policy) {
    struct p7r_uthread *uthread = 
        p7r_uthread_new(
                scheduler->index, 
                request.user_entrance, 
                request.user_argument, 
                &(scheduler->runners.stack_allocator),
                stack_alloc_policy);
    if (unlikely(uthread == NULL)) {
        if (request.user_argument_dtor)
            request.user_argument_dtor(request.user_argument);
        return NULL;
    }
    return uthread;

}

static inline
int p7r_uthread_request_is_null(struct p7r_uthread_request request) {
    return (request.user_entrance == NULL);
}

static inline
int swarm_sched_available(struct p7r_scheduler *scheduler) {
    if (scheduler->policy.swarm.enabled && scheduler->runners.tokens)
        return scheduler->runners.tokens--, 1;
    return 0;
}

static inline
void swarm_sched_refill(struct p7r_scheduler *scheduler) {
    (scheduler->runners.tokens < scheduler->policy.swarm.max_tokens) && (scheduler->runners.tokens++);
}

static
struct p7r_uthread *sched_resched_target(struct p7r_scheduler *scheduler) {
    if (scheduler->runners.running != NULL) {
        struct p7r_uthread *last_target = scheduler->runners.running;
        list_del(&(last_target->linkable));
        list_add_tail(&(last_target->linkable), &(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]));
    }
    // XXX it depends
    if (swarm_sched_available(scheduler) || list_is_empty(&(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]))) {
        struct p7r_uthread_request request = sched_cherry_pick(scheduler);
        if (p7r_uthread_request_is_null(request))
            return NULL;
        struct p7r_uthread *uthread = sched_uthread_from_request(scheduler, request, P7R_STACK_POLICY_DEFAULT);
        if (!uthread && request.user_argument_dtor)
            request.user_argument_dtor(request.user_argument);
        list_add_tail(&(uthread->linkable), &(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]));
    }
    list_ctl_t *target_reference = scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING].next;
    return scheduler->runners.running = container_of(target_reference, struct p7r_uthread, linkable);
}

static
void sched_idle(struct p7r_uthread *uthread) {
    p7r_context_switch(schedulers[uthread->scheduler_index].runners.carrier_context, &(uthread->context));
}

static
struct p7r_scheduler *p7r_scheduler_init(
        struct p7r_scheduler *scheduler, 
        uint32_t index, 
        uint32_t n_carriers, 
        struct p7r_context *carrier_context,
        struct p7r_stack_allocator_config config,
        int event_buffer_capacity) {
    __atomic_store_n(&(scheduler->status), P7R_SCHEDULER_BORN, __ATOMIC_RELEASE);
    __auto_type allocator = p7r_root_alloc_get_proxy();

    (scheduler->index = index), (scheduler->n_carriers = n_carriers);
    scheduler->runners.carrier_context = carrier_context;
    stack_allocator_init(&(scheduler->runners.stack_allocator), config);

    for (uint32_t queue_index = 0; queue_index < sizeof(scheduler->runners.sched_queues) / sizeof(list_ctl_t); queue_index++)
        init_list_head(&(scheduler->runners.sched_queues[queue_index]));
    init_list_head(&(scheduler->runners.request_queue));
    scheduler->runners.running = NULL;

    scheduler->bus.fd_epoll = epoll_create1(EPOLL_CLOEXEC);
    scheduler->bus.fd_notification = eventfd(0, EFD_CLOEXEC|EFD_NONBLOCK);
    {
        scheduler->bus.notification.checked_events.io.fd = scheduler->bus.fd_notification;
        scheduler->bus.notification.checked_events.io.epoll_event.events = EPOLLIN;
        scheduler->bus.notification.checked_events.io.epoll_event.data.ptr = &(scheduler->bus.notification);
        epoll_ctl(
                scheduler->bus.fd_epoll, 
                EPOLL_CTL_ADD, 
                scheduler->bus.notification.checked_events.io.fd, 
                &(scheduler->bus.notification.checked_events.io.epoll_event));
    }
    scheduler->bus.consumed = 1;
    scheduler->bus.message_boxes = scraft_allocate(allocator, sizeof(struct p7r_cpbuffer) * n_carriers);
    for (uint32_t message_box_index = 0; message_box_index < n_carriers; message_box_index++)
        cp_buffer_init(&(scheduler->bus.message_boxes[message_box_index]));
    scheduler->bus.foreign_message_box = &(scheduler->bus.message_boxes[index]);
    p7r_timer_queue_init(&(scheduler->bus.timers));
    scheduler->bus.n_epoll_events = event_buffer_capacity;      // XXX We do not check anything - keep your sanity
    scheduler->bus.epoll_events = scraft_allocate(allocator, sizeof(struct epoll_event) * event_buffer_capacity);
    (scheduler->bus.notification.checked_events.io.fd = scheduler->bus.fd_notification), (scheduler->bus.notification.uthread = NULL);

    __atomic_store_n(&(scheduler->status), P7R_SCHEDULER_ALIVE, __ATOMIC_RELEASE);

    return scheduler;
}

static
struct p7r_scheduler *p7r_scheduler_ruin(struct p7r_scheduler *scheduler) {
    __atomic_store_n(&(scheduler->status), P7R_SCHEDULER_DYING, __ATOMIC_RELEASE);
    __auto_type allocator = p7r_root_alloc_get_proxy();

    // All uthreads will be destroyed with the corresponding stack allocator
    stack_allocator_ruin(&(scheduler->runners.stack_allocator));

    scraft_deallocate(allocator, scheduler->bus.epoll_events);
    close(scheduler->bus.fd_epoll);
    close(scheduler->bus.fd_notification);
    {
        list_ctl_t *p, *t;
        for (uint32_t index = 0; index < 1; index++) {
            list_foreach_remove(p, &(scheduler->bus.message_boxes->buffers[index]), t) {
                list_del(t);
                struct p7r_internal_message *message = container_of(t, struct p7r_internal_message, linkable);
                if (message->content_destructor)
                    message->content_destructor(message);
            }
        }
    }
    scraft_deallocate(allocator, scheduler->bus.message_boxes);

    {
        list_ctl_t *p, *t;
        list_foreach_remove(p, &(scheduler->runners.request_queue), t) {
            list_del(t);
            p7r_uthread_request_delete(container_of(t, struct p7r_uthread_request, linkable));
        }
    }

    return scheduler;
}


// carriers

static
struct p7r_carrier *p7r_carrier_init(
        struct p7r_carrier *carrier, 
        uint32_t index, 
        pthread_t pthread_id, 
        struct p7r_scheduler *scheduler) {
    (carrier->index = index), (carrier->pthread_id = pthread_id), (carrier->scheduler = scheduler);
    return carrier;
}

static
struct p7r_carrier *p7r_carrier_ruin(struct p7r_carrier *carrier) {
    // XXX empty implementation
    return carrier;
}

static
void *p7r_carrier_lifespan(void *self_argument) {
    struct p7r_carrier *self = self_argument;
    self_carrier = self;

    pthread_barrier_wait(&carrier_barrier);

    struct p7r_scheduler *scheduler = self->scheduler;
    while (1) {     // TODO self-destruct
        sched_bus_refresh(scheduler);
        struct p7r_uthread_request request = sched_cherry_pick(scheduler);
        if (request.user_entrance) {
            struct p7r_uthread *uthread = sched_uthread_from_request(scheduler, request, P7R_STACK_POLICY_DEFAULT);
            if (uthread)
                list_add_tail(&(uthread->linkable), &(scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]));
            else if (request.user_argument_dtor)
                request.user_argument_dtor(request.user_argument);
        }
        struct p7r_uthread *target = sched_resched_target(scheduler);
        if (target)
            p7r_context_switch(&(target->context), &(self->context));
    }

    return NULL;
}


// iuc & u2cc

static
void p7r_internal_message_delete(struct p7r_internal_message *message) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    scraft_deallocate(allocator, message);
}

static
struct p7r_internal_message *p7r_u2cc_message_raw(uint64_t base_type, size_t size_hint) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    struct p7r_internal_message *message = scraft_allocate(allocator, P7R_BUFFERED_MESSAGE_SIZE(size_hint));
    if (unlikely(message == NULL))
        return NULL;
    message->type = base_type|P7R_INTERNAL_U2CC;
    return message;
}

static
void p7r_u2cc_message_post(uint32_t dst_index, uint32_t src_index, struct p7r_internal_message *message) {
    struct p7r_scheduler *destination = carriers[dst_index].scheduler;
    cp_buffer_produce(&(destination->bus.message_boxes[src_index]), &(message->linkable));
    {
        uint64_t event_notification = 1;
        write(destination->bus.fd_notification, &event_notification, sizeof(uint64_t));
    }
}


// api & basement

static
int p7r_uthread_create_(void (*entrance)(void *), void *argument, void (*dtor)(void *)) {
    uint32_t target_carrier_index = next_balance_index;
    uint32_t n_carriers = self_carrier->scheduler->n_carriers;

    int remote_created;

    if (remote_created = (target_carrier_index % n_carriers != self_carrier->index)) {
        struct p7r_internal_message *request_message = p7r_u2cc_message_raw(P7R_MESSAGE_UTHREAD_REQUEST, sizeof(struct p7r_uthread_request));
        if (unlikely(request_message == NULL))
            return -1;
        struct p7r_uthread_request *request = (struct p7r_uthread_request *) &(request_message->content_buffer);
        (request->user_entrance = entrance), (request->user_argument = argument), (request->user_argument_dtor = dtor);
        p7r_u2cc_message_post(target_carrier_index % n_carriers, self_carrier->index, request_message);
    } else {
        struct p7r_uthread_request request = { .user_entrance = entrance, .user_argument = argument };
        struct p7r_uthread *uthread = sched_uthread_from_request(self_carrier->scheduler, request, P7R_STACK_POLICY_DEFAULT);
        if (uthread)
            list_add_tail(&(uthread->linkable), &(self_carrier->scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]));
    }

    return remote_created;
}

static
void p7r_blocking_point(void) {
    struct p7r_scheduler *self_scheduler = self_carrier->scheduler;
    struct p7r_uthread *self = self_scheduler->runners.running;

    list_del(&(self->linkable));
    // TODO refactor - extract common code snippet
    p7r_uthread_change_state_clean(self, P7R_UTHREAD_BLOCKING);
    list_add_tail(&(self->linkable), &(self_scheduler->runners.sched_queues[P7R_SCHED_QUEUE_BLOCKING]));
    self_scheduler->runners.running = NULL;

    struct p7r_uthread *target;
    do {
        sched_bus_refresh(self_scheduler);
        // FIXME force reload uthread within resource bound
        target = sched_resched_target(self_scheduler);
    } while (target == NULL);

    if (target != self)
        p7r_uthread_switch(target, self);
}

static inline
int p7r_delegation_io_based(struct p7r_scheduler *scheduler, struct p7r_delegation *delegation, int fd) {
    int ret = 1;
    delegation->checked_events.io.fd = fd;
    delegation->checked_events.io.epoll_event.events = 
        ((delegation->p7r_event & P7R_DELEGATION_READ) ? EPOLLIN : 0) |
        ((delegation->p7r_event & P7R_DELEGATION_WRITE) ? EPOLLOUT : 0) |
        EPOLLONESHOT;
    delegation->checked_events.io.epoll_event.data.ptr = delegation;
    int fd_epoll = scheduler->bus.fd_epoll;
    if (epoll_ctl(fd_epoll, EPOLL_CTL_MOD, fd, &(delegation->checked_events.io.epoll_event)) == -1)
        if (errno == ENOENT)
            (epoll_ctl(fd_epoll, EPOLL_CTL_ADD, fd,&(delegation->checked_events.io.epoll_event)) == 0) || (ret = 0);
    ret && ((delegation->checked_events.io.enabled = 1), (delegation->checked_events.io.triggered = 0));
    return ret;
}

static inline
int p7r_delegation_iuc_based(struct p7r_scheduler *scheduler, struct p7r_delegation *delegation) {
    // TODO implementation
    return 0;
}

static inline
int p7r_delegation_timed(struct p7r_scheduler *scheduler, struct p7r_delegation *delegation, uint64_t dt) {
    p7r_timer_core_init_diff(&(delegation->checked_events.timer.measurement), dt, scheduler->runners.running);
    (delegation->checked_events.timer.enabled = 1), (delegation->checked_events.timer.triggered = 0);
    return 1;
}

int p7r_uthread_create_foreign(uint32_t index, void (*entrance)(void *), void *argument, void (*dtor)(void *)) {
    uint32_t target_carrier_index = next_balance_index;
    uint32_t n_carriers = carriers[target_carrier_index].scheduler->n_carriers;

    struct p7r_internal_message *request_message = p7r_u2cc_message_raw(P7R_MESSAGE_UTHREAD_REQUEST, sizeof(struct p7r_uthread_request));
    if (unlikely(request_message == NULL))
        return -1;
    struct p7r_uthread_request *request = (struct p7r_uthread_request *) &(request_message->content_buffer);
    (request->user_entrance = entrance), (request->user_argument = argument), (request->user_argument_dtor = dtor);
    p7r_u2cc_message_post(target_carrier_index % n_carriers, index, request_message);

    return 0;
}

void p7r_yield(void) {
    struct p7r_scheduler *self_scheduler = self_carrier->scheduler;
    struct p7r_uthread *self = self_scheduler->runners.running;
    swarm_sched_refill(self_scheduler);
    sched_bus_refresh(self_scheduler);
    struct p7r_uthread *target = sched_resched_target(self_scheduler);
    p7r_uthread_switch(target, self);
}

int p7r_uthread_create(void (*entrance)(void *), void *argument, void (*dtor)(void *), int yield) {
    int remote_created = p7r_uthread_create_(entrance, argument, dtor);
    if (yield && !remote_created)
        p7r_yield();
    return remote_created;
}

struct p7r_delegation p7r_delegate(uint64_t events, ...) {
    struct p7r_scheduler *self_scheduler = self_carrier->scheduler;
    struct p7r_delegation delegation = { .uthread = self_scheduler->runners.running };

    delegation.p7r_event = events;

    va_list arguments;
    va_start(arguments, events);

    if (events & (P7R_DELEGATION_READ|P7R_DELEGATION_WRITE)) 
        p7r_delegation_io_based(self_scheduler, &delegation, va_arg(arguments, int));

    if (events & P7R_DELEGATION_ALLOW_OOB)
        p7r_delegation_iuc_based(self_scheduler, &delegation);

    if (events & P7R_DELEGATION_TIMED)
        p7r_delegation_timed(self_scheduler, &delegation, va_arg(arguments, uint64_t));

    va_end(arguments);

    // XXX as-fair-as-possible schedule
    p7r_blocking_point();

    return delegation;
}

int p7r_init(struct p7r_config config) {
    srand((unsigned) time(NULL));

    {
        __auto_type allocator_real = p7r_root_alloc_get_allocator();
        allocator_real->allocator_.closure_ = config.root_allocator.allocate;
        allocator_real->deallocator_.closure_ = config.root_allocator.deallocate;
        allocator_real->reallocator_.closure_ = config.root_allocator.reallocate;
    }

    __auto_type allocator = p7r_root_alloc_get_proxy();
    schedulers = scraft_allocate(allocator, sizeof(struct p7r_scheduler) * config.concurrency.n_carriers);
    carriers = scraft_allocate(allocator, sizeof(struct p7r_carrier) * config.concurrency.n_carriers);
    if (!schedulers || !carriers) {
        (!schedulers && (scraft_deallocate(allocator, schedulers), 0)), (!carriers && (scraft_deallocate(allocator, carriers), 0));
        return -1;
    }
    for (uint32_t index = 0; index < config.concurrency.n_carriers; index++) {
        (carriers[index].index = index), (carriers[index].scheduler = &(schedulers[index]));
        p7r_scheduler_init(
                &(schedulers[index]), 
                index, 
                config.concurrency.n_carriers, 
                &(carriers[index].context), 
                config.stack_allocator, 
                config.concurrency.event_buffer_capacity
        );
        // TODO init policy
        (schedulers[index].policy.swarm.enabled = config.concurrency.swarm.enabled),
            (schedulers[index].policy.swarm.max_tokens = config.concurrency.swarm.max_tokens);
    }
    {
        pthread_barrierattr_t barrier_attribute;
        pthread_barrierattr_init(&barrier_attribute);
        pthread_barrier_init(&carrier_barrier, &barrier_attribute, config.concurrency.n_carriers);
    }
    {
        pthread_attr_t detach_attr;
        pthread_attr_init(&detach_attr);
        pthread_attr_setdetachstate(&detach_attr, PTHREAD_CREATE_DETACHED);
        for (uint32_t index = 1; index < config.concurrency.n_carriers; index++)
            pthread_create(&(carriers[index].pthread_id), &detach_attr, p7r_carrier_lifespan, &(carriers[index]));
    }
    
    struct p7r_stack_metamark *main_sched_stack = 
        stack_metamark_create(&(carriers[0].scheduler->runners.stack_allocator), P7R_STACK_POLICY_DEFAULT);
    p7r_context_init(&(carriers[0].context), stack_meta_of(main_sched_stack), stack_size_of(main_sched_stack));
    p7r_context_prepare(&(carriers[0].context), (void (*)(void *)) p7r_carrier_lifespan, &(carriers[0]));
    list_add_tail(&(main_uthread.linkable), &(carriers[0].scheduler->runners.sched_queues[P7R_SCHED_QUEUE_RUNNING]));
    carriers[0].scheduler->runners.running = &main_uthread;

    p7r_context_switch(&(carriers[0].context), &(main_uthread.context));

    return 0;
}
