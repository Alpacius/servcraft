#ifndef     P7R_UTHREAD_H_
#define     P7R_UTHREAD_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

#include    "./p7r_timing.h"
#include    "./p7r_cpbuffer.h"
#include    "./p7r_stack_allocator_adaptor.h"
#include    "./p7r_context.h"


struct p7r_uthread {
    uint32_t scheduler_index;
    struct p7r_context context;
    struct p7r_stack_metamark *stack_metamark;
    uint64_t status;
    struct {
        void (*user_entrance)(void *);
        void (*real_entrance)(void *);
        void *user_argument, *real_argument;
    } entrance;
    list_ctl_t linkable;
};

struct p7r_uthread_request {
    void (*user_entrance)(void *);
    void *user_argument;
    void (*user_argument_dtor)(void *);
    list_ctl_t linkable;
};

#define     P7R_UTHREAD_PRELAUNCH       0
#define     P7R_UTHREAD_RUNNING         1
#define     P7R_UTHREAD_LIMBO           2
#define     P7R_UTHREAD_DYING           3
#define     P7R_UTHREAD_BLOCKING        4
#define     P7R_UTHREAD_IO_READY        (1 << 4)
#define     P7R_UTHREAD_COMMU_READY     (1 << 5)

#define     P7R_UTHREAD_STATUS_MASK     7

struct p7r_timer_core {
    uint64_t timestamp;
    int triggered;
    struct p7r_uthread *uthread;
    struct scraft_rbtree_node maplink;
};

struct p7r_timer_queue {
    struct scraft_rbtree map;
};

struct p7r_delegation {
    uint64_t p7r_event;
    struct {
        struct {
            int fd;
            struct epoll_event epoll_event;
            int triggered, enabled;
        } io;
        struct {
            int triggered, enabled;
        } oob;
        struct {
            struct p7r_timer_core measurement;
            int triggered, enabled;
        } timer;
    } checked_events;
    struct p7r_uthread *uthread;
};

#define     P7R_DELEGATION_BASE         0
#define     P7R_DELEGATION_READ         1
#define     P7R_DELEGATION_WRITE        2
#define     P7R_DELEGATION_ALLOW_OOB    (1 << 4)
#define     P7R_DELEGATION_TIMED        (1 << 5)

#define     P7R_N_SCHED_QUEUES          3
#define     P7R_SCHED_QUEUE_RUNNING     0
#define     P7R_SCHED_QUEUE_BLOCKING    1
#define     P7R_SCHED_QUEUE_DYING       2

struct p7r_scheduler {
    uint32_t index;
    uint32_t n_carriers;
    uint64_t status;
    struct {
        list_ctl_t request_queue;
        list_ctl_t sched_queues[P7R_N_SCHED_QUEUES];
        struct p7r_uthread *running;
        struct p7r_context *carrier_context;
        struct p7r_stack_allocator stack_allocator;
    } runners;
    struct {
        int fd_epoll;
        int fd_notification;
        struct p7r_delegation notification;
        int consumed;
        struct p7r_cpbuffer *message_boxes;
        struct p7r_timer_queue timers;
        struct epoll_event *epoll_events;
        int n_epoll_events;
    } bus;
};

#define     P7R_SCHEDULER_BORN          0
#define     P7R_SCHEDULER_ALIVE         1
#define     P7R_SCHEDULER_DYING         2

struct p7r_carrier {
    uint32_t index;
    pthread_t pthread_id;
    struct p7r_context context;
    struct p7r_scheduler *scheduler;
};

#define     P7R_INTERNAL_U2CC               0x1         // vs. IUC
#define     P7R_INTERNAL_ATTACHED           0x2         // vs. BUFFERED
#define     P7R_MESSAGE_UNDEFINED           (0 << 2)
#define     P7R_MESSAGE_UTHREAD_REQUEST     (1 << 2)

#define     P7R_MESSAGE_REAL_TYPE(type_)    (((type_) & ~3) >> 2)

struct p7r_internal_message {
    uint64_t type;
    uint32_t from, to;
    list_ctl_t linkable, communicatable;
    void (*content_destructor)(struct p7r_internal_message *);
    void *(*content_extractor)(struct p7r_internal_message *);
    char content_buffer;
} __attribute__((packed));

#define     P7R_BUFFERED_MESSAGE_SIZE(size_)    (sizeof(struct p7r_internal_message) - sizeof(char) + (size_))
#define     P7R_MESSAGE_OF(buffer_)             container_of(((char *) buffer_), struct p7r_internal_message, content_buffer)

struct p7r_config {
    struct {
        uint32_t n_carriers;
        int event_buffer_capacity;
    } concurrency;
    struct {
        void *(*allocate)(size_t);
        void (*deallocate)(void *);
        void *(*reallocate)(void *, size_t);
    } root_allocator;
    struct p7r_stack_allocator_config stack_allocator;
};

int p7r_init(struct p7r_config config);
struct p7r_delegation p7r_delegate(uint64_t events, ...);
void p7r_yield(void);
int p7r_uthread_create(void (*entrance)(void *), void *argument, void (*dtor)(void *), int yield);

#endif      // P7R_UTHREAD_H_
