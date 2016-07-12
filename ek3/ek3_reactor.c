#include    "./ek3_reactor.h"

static inline
uint64_t get_timeval_current(void) {
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME_COARSE, &tv);
    return ((uint64_t) tv.tv_sec * 1000) + ((uint64_t) tv.tv_nsec / 1000000);
}

static inline
uint64_t salt_simple(void) {
    static uint64_t salt_counter = 0;
    return __atomic_fetch_add(&salt_counter, 1, __ATOMIC_ACQ_REL);
}

static
int ek3_event_key_compare(const void *lhs_, const void *rhs_) {
    uint64_t lhs = *((uint64_t *) lhs), rhs = *((uint64_t *) rhs);
    return (lhs == rhs) ? 0 : ((lhs < rhs) ? -1 : 1);
}

static inline
void ek3_event_io_decorate(struct ek3_event *event, int type) {
    // TODO implementation
    if (event->type & EK3_EVENTFLAG_ET)
        event->epoll_info.events |= EPOLLET;
    if (event->type & EK3_EVENTFLAG_RDHUP)
        event->epoll_info.events |= EPOLLRDHUP;
}

static
struct ek3_event *ek3_event_init(struct ek3_event *event, int type, int fd, void *(*handler)(struct ek3_event *, void *), void (*param_dtor)(void *), void *handler_user_param, va_list arguments) {
    int epoll_event_mapping[] = {
        [EK3_EVENT_READ] = EPOLLIN,
        [EK3_EVENT_WRITE] = EPOLLOUT,
        [EK3_EVENT_ERROR] = EPOLLERR, // unnecessary
    };
    event->type = type;
    event->fd = fd;
    event->handler = handler;
    event->param_dtor = param_dtor;
    event->handler_user_param = handler_user_param;
    event->deprecated = 1;
    switch (type & ~EK3_EVENTFLAG_ALL) {
        case EK3_EVENT_READ:
        case EK3_EVENT_WRITE:
        case EK3_EVENT_ERROR:
            {
                event->epoll_info.events = epoll_event_mapping[type & ~EK3_EVENTFLAG_ALL];
                event->epoll_info.data.ptr = event;
            }
            break;
        case EK3_EVENT_TIMEOUT:
            {
                event->timeout_stamp = 0;
                event->timeout_period = va_arg(arguments, int32_t);
                event->timer_reference.key_ref = &(event->timeout_stamp);
            }
            break;
        default:
            // FIXME hohoho
            ;
    }
    return event;
}

static
struct ek3_event *ek3_event_ruin(struct ek3_event *event) {
    if (event->deprecated == 0) {
        event->deprecated = 1;
        scraft_rbt_detach(&(event->timer_reference));
    }
    if (event->param_dtor && event->handler_user_param)
        event->param_dtor(event->handler_user_param);
    return event;
}

static
struct ek3_event *ek3_event_new(void) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    return scraft_allocate(allocator, sizeof(struct ek3_event));
}

static
void ek3_event_delete(struct ek3_event *event) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    scraft_deallocate(allocator, event);
}

struct ek3_event *ek3_event_create(int type, int fd, void *(*handler)(struct ek3_event *, void *), void (*param_dtor)(void *), void *handler_user_param, ...) {
    struct ek3_event *event = ek3_event_new();
    if (event) {
        va_list arguments;
        va_start(arguments, handler_user_param);
        ek3_event_init(event, type, fd, handler, param_dtor, handler_user_param, arguments);
        va_end(arguments);
    }
    return event;
}

void ek3_event_destroy(struct ek3_event *event) {
    ek3_event_delete(ek3_event_ruin(event));
}

static inline
list_ctl_t *ek3_event_node_reference(struct ek3_event *event) {
    return &(event->node_reference);
}

static inline
struct scraft_rbtree_node *ek3_event_timer_reference(struct ek3_event *event) {
    return &(event->timer_reference);
}

static
struct ek3_session *ek3_session_init_(struct ek3_session *session, int fd, const uint8_t *client_address, size_t address_length) {
    session->fd = fd;
    session->status = EK3_SESSION_NOT_ACTIVATED;
    init_list_head(&(session->events_notified));
    init_list_head(&(session->events_registered));
    init_list_head(&(session->events_deployed));
    {   // MD5 session id
        char session_id_buffer[EK3_SESSION_ID_BUFFER_LENGTH];
        char md5_cstring_buffer[MD5_DIGEST_STRING_LENGTH];
        MD5Data(client_address, address_length, md5_cstring_buffer);
        int raw_id_length = sprintf(session_id_buffer, "%s-%PRIu64-%PRIu64", md5_cstring_buffer, get_timeval_current(), salt_simple());
        MD5Data(session_id_buffer, raw_id_length, session->id);
    }
    return session;
}

static
struct ek3_session *ek3_session_ruin(struct ek3_session *session) {
    close(session->fd);
    list_ctl_t *p, *t;
    list_foreach_remove(p, &(session->events_registered), t) {
        list_del(t);
        ek3_event_delete(ek3_event_ruin(container_of(t, struct ek3_event, node_reference)));
    }
    list_foreach_remove(p, &(session->events_deployed), t) {
        list_del(t);
        ek3_event_delete(ek3_event_ruin(container_of(t, struct ek3_event, node_reference)));
    }
    list_foreach_remove(p, &(session->events_notified), t) {
        list_del(t);
        ek3_event_delete(ek3_event_ruin(container_of(t, struct ek3_event, node_reference)));
    }
    return session;
}

static
struct ek3_session *ek3_session_init_v4(struct ek3_session *session, int fd, struct sockaddr_in *client_address) {
    return session ? ek3_session_init_(session, fd, (const void *) client_address, sizeof(struct sockaddr_in)) : NULL;
}

static
struct ek3_session *ek3_session_init_v6(struct ek3_session *session, int fd, struct sockaddr_in6 *client_address) {
    return session ? ek3_session_init_(session, fd, (const void *) client_address, sizeof(struct sockaddr_in6)) : NULL;
}

static
struct ek3_session *ek3_session_new(void) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    return scraft_allocate(allocator, sizeof(struct ek3_session));
}

static
void ek3_session_delete(struct ek3_session *session) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    scraft_deallocate(allocator, session);
}

struct ek3_session *ek3_session_create_v4(int fd, struct sockaddr_in *client_address) {
    return ek3_session_init_v4(ek3_session_new(), fd, client_address);
}

struct ek3_session *ek3_session_create_v6(int fd, struct sockaddr_in6 *client_address) {
    return ek3_session_init_v6(ek3_session_new(), fd, client_address);
}

void ek3_session_destroy(struct ek3_session *session) {
    ek3_session_delete(ek3_session_ruin(session));
}

void ek3_session_detach(struct ek3_session *session) {
    scraft_hashtable_remove(session->parent_reactor->sessions, &(session->hash_reference));
}

static
int ek3_session_hash_compare(struct scraft_hashkey *lhs_, struct scraft_hashkey *rhs_) {
    struct ek3_session *lhs = container_of(lhs_, struct ek3_session, hash_reference),
                       *rhs = container_of(rhs_, struct ek3_session, hash_reference);
    return strcmp(lhs->id, rhs->id);
}

static
uint64_t ek3_session_hash_function(struct scraft_hashkey *key) {
    struct ek3_session *session = container_of(key, struct ek3_session, hash_reference);
    return scraft_hashaux_djb_cstring(session->id);
}

static
int ek3_session_hash_destroy(struct scraft_hashkey *key) {
    struct ek3_session *session = container_of(key, struct ek3_session, hash_reference);
    ek3_session_destroy(session);
    return 0;
}

void ek3_session_register_event(struct ek3_session *session, struct ek3_event *event) {
    list_add_tail(ek3_event_node_reference(event), &(session->events_registered));
    event->parent_session = session;
}

static inline
int ek3_session_full_status(struct ek3_session *session, int status) {
    int session_status_flags = session->status & EK3_SESSIONFLAG_ALL;
    return session_status_flags|status;
}

static inline
int ek3_session_unmask_status(struct ek3_session *session, int base_status, int unmask_flags) {
    return base_status&~unmask_flags;
}

static inline
int ek3_session_mask_status(struct ek3_session *session, int mask_flags) {
    return session->status|mask_flags;
}

static inline
list_ctl_t *ek3_session_node_reference(struct ek3_session *session) {
    return &(session->node_reference);
}

struct ek3_event *ek3_session_next_event_notified(struct ek3_session *session) {
    struct ek3_event *event = NULL;
    if (!list_is_empty(&(session->events_notified))) {
        event = container_of(session->events_notified.next, struct ek3_event, node_reference);
        list_del(&(event->node_reference));
    }
    return event;
}

static
struct ek3_reactor *ek3_reactor_init(struct ek3_reactor *reactor, uint32_t epoll_capacity, uint64_t hash_capacity) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    reactor->epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    reactor->notified = 0;
    reactor->epoll_capacity = epoll_capacity;
    init_list_head(&(reactor->session_queue_notified));
    init_list_head(&(reactor->session_queue_committing));
    init_list_head(&(reactor->session_queue_posted));
    init_list_head(&(reactor->session_queue_deployed));
    scraft_rbt_init(&(reactor->timer_queue), ek3_event_key_compare);
    reactor->sessions = scraft_hashtable_new(allocator, hash_capacity, ek3_session_hash_compare, ek3_session_hash_destroy, ek3_session_hash_function);
    return reactor;
}

static
struct ek3_reactor *ek3_reactor_ruin(struct ek3_reactor *reactor) {
    close(reactor->epoll_fd);
    reactor->notified = 0;
    scraft_hashtable_destroy(reactor->sessions);    // all sessions & events will be destroyed
    return reactor;
}

static
struct ek3_reactor *ek3_reactor_new(uint32_t epoll_capacity) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    return scraft_allocate(allocator, (sizeof(struct ek3_reactor) + sizeof(struct epoll_event) * epoll_capacity));
}

static
void ek3_reactor_delete(struct ek3_reactor *reactor) {
    __auto_type allocator = ek3_root_alloc_get_proxy();
    scraft_deallocate(allocator, reactor);
}

struct ek3_reactor *ek3_reactor_create(uint32_t epoll_capacity, uint64_t hash_capacity) {
    struct ek3_reactor *reactor = ek3_reactor_new(epoll_capacity);
    return reactor ? ek3_reactor_init(reactor, epoll_capacity, hash_capacity) : NULL;
}

void ek3_reactor_destroy(struct ek3_reactor *reactor) {
    list_ctl_t *p, *t;
    list_foreach_remove(p, &(reactor->session_queue_notified), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        ek3_session_destroy(session);
    }
    list_foreach_remove(p, &(reactor->session_queue_committing), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        ek3_session_destroy(session);
    }
    list_foreach_remove(p, &(reactor->session_queue_posted), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        ek3_session_destroy(session);
    }
    list_foreach_remove(p, &(reactor->session_queue_deployed), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        ek3_session_destroy(session);
    }
    ek3_reactor_delete(ek3_reactor_ruin(reactor));
}

int ek3_commit_session(struct ek3_reactor *reactor, struct ek3_session *session) {
    int session_status = session->status & ~EK3_SESSIONFLAG_ALL;
    if (session_status == EK3_SESSION_COMMITTED)
        return 0;
    session->status = ek3_session_full_status(session, EK3_SESSION_COMMITTED);
    session->parent_reactor = reactor;
    list_add_tail(ek3_session_node_reference(session), &(reactor->session_queue_committing));
    return 1;
}

void ek3_register_session(struct ek3_reactor *reactor, struct ek3_session *session) {
    scraft_hashtable_insert(reactor->sessions, &(session->hash_reference));
}

static
void ek3_deploy_event(struct ek3_reactor *reactor, struct ek3_event *event, uint64_t current_time) {
    switch (event->type & ~EK3_EVENTFLAG_ALL) {
        case EK3_EVENT_READ:
        case EK3_EVENT_WRITE:
        case EK3_EVENT_ERROR:
            {
                // XXX ignore EEXIST
                epoll_ctl(reactor->epoll_fd, EPOLL_CTL_ADD, event->fd, &(event->epoll_info));
            }
            break;
        case EK3_EVENT_TIMEOUT:
            {
                scraft_rbt_insert(&(reactor->timer_queue), ek3_event_timer_reference(event));
            }
            break;
        default:
            ;
    }
}

static
void ek3_deploy_session(struct ek3_reactor *reactor, struct ek3_session *session, uint64_t current_time) {
    list_ctl_t *p, *t;
    list_add_tail(ek3_session_node_reference(session), &(reactor->session_queue_deployed));
    list_foreach_remove(p, &(session->events_registered), t) {
        list_del(t);
        list_add_tail(t, &(session->events_deployed));
        struct ek3_event *event = container_of(t, struct ek3_event, node_reference);
        ek3_deploy_event(reactor, event, current_time);
    }
}

static
void ek3_dispatch_event(struct ek3_reactor *reactor, struct ek3_event *event) {
    switch (event->type & ~EK3_EVENTFLAG_ALL) {
        case EK3_EVENT_READ:
        case EK3_EVENT_WRITE:
        case EK3_EVENT_ERROR:
            {
                // XXX set posted event
                if (event->type & EK3_EVENTFLAG_POSTED)
                    event->parent_session->status = ek3_session_mask_status(event->parent_session, EK3_SESSIONFLAG_POSTED);
            }
            break;
        case EK3_EVENT_TIMEOUT:
            {
            }
            break;
    }
    if (!(event->parent_session->status & EK3_SESSION_ACTIVATED)) {
        list_del(ek3_session_node_reference(event->parent_session));
        if (event->parent_session->status & EK3_SESSIONFLAG_POSTED)
            list_add_tail(ek3_session_node_reference(event->parent_session), &(reactor->session_queue_posted));
        else
            list_add_tail(ek3_session_node_reference(event->parent_session), &(reactor->session_queue_notified));
    }
    event->parent_session->status = ek3_session_full_status(event->parent_session, EK3_SESSION_ACTIVATED);

    // XXX should we move this modification to ek3_deploy_session?
    list_del(ek3_event_node_reference(event));
    list_add_tail(ek3_event_node_reference(event), &(event->parent_session->events_notified));
}

static inline
struct ek3_event *timer_peek_earliest(struct ek3_reactor *reactor) {
    struct scraft_rbtree_node *target = scraft_rbtree_min(&(reactor->timer_queue));
    return (target != reactor->timer_queue.sentinel) ? container_of(target, struct ek3_event, timer_reference) : NULL;
}

struct ek3_session *ek3_poll(struct ek3_reactor *reactor) {
    uint64_t baseline_time = get_timeval_current();
    int epoll_timeout_period = -1;

    list_ctl_t *p, *t;
    list_foreach_remove(p, &(reactor->session_queue_committing), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        ek3_deploy_session(reactor, session, baseline_time);     // timestamp set based on current_time
    }

    struct ek3_session *ek3_poll_local_(void) {
        list_ctl_t *target_reference = reactor->session_queue_notified.next;
        list_del(target_reference);
        struct ek3_session *session = container_of(target_reference, struct ek3_session, node_reference);
        return session;
    }
    if (!list_is_empty(&(reactor->session_queue_notified)))
        return ek3_poll_local_();

    struct ek3_event *earliest = timer_peek_earliest(reactor);
    if (earliest) {
        epoll_timeout_period = earliest->timeout_stamp - baseline_time;
        (epoll_timeout_period < 0) && (epoll_timeout_period = 0);
    }
    if (!list_is_empty(&(reactor->session_queue_posted)))
        epoll_timeout_period = 0;       // epoll_wait returns immediately if there's any posted events

    int n_active_events = epoll_wait(reactor->epoll_fd, reactor->epoll_triggered_events, reactor->epoll_capacity, epoll_timeout_period);
    if (n_active_events < 0) {
        return NULL;
    }

    uint64_t deadline_time = get_timeval_current();
    struct ek3_event *timer_event_iterator = NULL;
    while ((timer_event_iterator = timer_peek_earliest(reactor)) != NULL) {
        if (timer_event_iterator->timeout_stamp > deadline_time)
            break;
        ek3_dispatch_event(reactor, timer_event_iterator);
    }

    for (int index_active_events = 0; index_active_events < n_active_events; index_active_events++)
        ek3_dispatch_event(reactor, (struct ek3_event *) (reactor->epoll_triggered_events[index_active_events].data.ptr));

    list_foreach_remove(p, &(reactor->session_queue_posted), t) {
        list_del(t);
        struct ek3_session *session = container_of(t, struct ek3_session, node_reference);
        // FIXME implementation
        session->status = ek3_session_unmask_status(session, EK3_SESSION_NOT_ACTIVATED, EK3_SESSIONFLAG_POSTED);
        list_add_tail(ek3_session_node_reference(session), &(reactor->session_queue_notified));
    }

    return ek3_poll_local_();
}
