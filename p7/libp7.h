#ifndef     _LIBP7_H_
#define     _LIBP7_H_

#include    "./p7impl.h"
#include    "./spin.h"
#include    "./rwspin.h"

void p7_coro_yield(void);
void p7_coro_create(void (*entry)(void *), void *arg, size_t stack_size);
void p7_timed_event(uint64_t dt, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *)));
struct p7_timer_event *p7_timed_event_assoc(uint64_t dt, void (*func)(void *), void *arg, void (*dtor)(void *, void (*)(void *)));
int p7_iowrap_(int fd, int rdwr);
unsigned p7_timedout_(void);
unsigned p7_timeout_reset(void);
void p7_timer_clean_(struct p7_timer_event *ev);
int p7_init(unsigned nthreads, void (*at_startup)(void *), void *arg);
void p7_coro_concat(void (*entry)(void *), void *arg, size_t stack_size);

#define p7_iowrap(_fn_, _rdwr_, _fd_, ...) \
({ \
    int fd_ = (_fd_), rdwr_ = (_rdwr_); \
    __auto_type fn_ = (_fn_); \
    p7_iowrap_(fd_, rdwr_); \
    fn_(fd_, __VA_ARGS__); \
})

#define p7_io_notify(_fd_, _rdwr_) \
do { \
    p7_iowrap_(_fd_, _rdwr_); \
} while (0)

#define p7_iowrap_timed(_fn_, _rdwr_, _dt_, _fd_, ...) \
({ \
    int fd_ = (_fd_), rdwr_ = (_rdwr_); \
    uint64_t dt_ = (_dt_); \
    __auto_type fn_ = (_fn_); \
    __auto_type ev_ = p7_timed_event_assoc(dt_, NULL, NULL, NULL); \
    p7_iowrap_(fd_, rdwr_); \
    int ret_; \
    if (p7_timedout_()) { \
        ret_ = -2; \
        p7_timeout_reset(); \
    } \
    else { \
        p7_timer_clean_(ev_); \
        ret_ = fn_(fd_, __VA_ARGS__); \
    } \
    (volatile int) ret_; \
})

void p7_send_by_entity(void *dst, struct p7_msg *msg);
struct p7_msg *p7_recv(void);
int p7_send_by_name(const char *name, struct p7_msg *msg);
void *p7_coro_register_name(const char *name);
void p7_coro_discard_name(void *name_handle);

#define p7_recv_timed(dt_) \
({ \
    __auto_type ev_ = p7_timed_event_assoc(dt_, NULL, NULL, NULL); \
    struct p7_msg *msg_ = p7_recv(); \
    if (p7_timedout_()) \
        p7_timeout_reset(); \
    else \
        p7_timer_clean_(ev_); \
    msg_; \
})

void p7_coro_set_mailbox_cleaner(void (*cleaner)(void *));
void p7_coro_set_mailbox_cleaner_arg(void *arg);
void *p7_coro_get_mailbox_cleaner_arg(void);
struct p7_msg *p7_mailbox_extract(void);

#endif      // _LIBP7_H_
