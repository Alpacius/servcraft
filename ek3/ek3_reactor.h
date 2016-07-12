#ifndef     EK3_REACTOR_H_
#define     EK3_REACTOR_H_

#include    <stddef.h>
#include    <stdint.h>
#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    <stdarg.h>

#define     __STDC_FORMAT_MACROS
#include    <inttypes.h>

#include    <unistd.h>
#include    <errno.h>
#include    <sys/types.h>
#include    <bsd/md5.h>
#include    <sys/stat.h>
#include    <time.h>
#include    <sys/socket.h>
#include    <arpa/inet.h>
#include    <netinet/in.h>
#include    <sys/epoll.h>

#include    "../include/miscutils.h"
#include    "../include/util_list.h"
#include    "../include/model_alloc.h"
#include    "../util/scraft_rbt_ifce.h"
#include    "../util/scraft_hashtable_ifce.h"

#include    "./ek3_root_alloc.h"

struct ek3_reactor {
    int epoll_fd;
    uint64_t notified;
    list_ctl_t session_queue_notified, session_queue_committing, session_queue_posted, session_queue_deployed;
    struct scraft_rbtree timer_queue;
    struct scraft_hashtable *sessions;
    uint32_t epoll_capacity;
    struct epoll_event epoll_triggered_events[];
};

#define     EK3_SESSION_ID_STRLEN           (MD5_DIGEST_STRING_LENGTH)
#define     EK3_SESSION_ID_BUFFER_LENGTH    (MD5_DIGEST_STRING_LENGTH + 128)

#define     EK3_N_EVENT_TYPES               5
#define     EK3_EVENT_NONE                  0 
#define     EK3_EVENT_READ                  1
#define     EK3_EVENT_WRITE                 2
#define     EK3_EVENT_ERROR                 3
#define     EK3_EVENT_TIMEOUT               4
#define     EK3_EVENTFLAG_RDHUP             16
#define     EK3_EVENTFLAG_POSTED            32
#define     EK3_EVENTFLAG_ET                64
#define     EK3_EVENTFLAG_ONESHOT           128

#define     EK3_EVENTFLAG_ALL \
    (EK3_EVENTFLAG_RDHUP|EK3_EVENTFLAG_POSTED|EK3_EVENTFLAG_ET|EK3_EVENTFLAG_ONESHOT)

struct ek3_session;

struct ek3_event {
    int type;
    int fd;
    int deprecated;
    void *(*handler)(struct ek3_event *, void *);
    void (*param_dtor)(void *);
    void *handler_user_param;
    uint64_t timeout_stamp;
    int32_t timeout_period;     // epoll_wait
    struct scraft_rbtree_node timer_reference;
    struct epoll_event epoll_info;
    struct ek3_session *parent_session;
    list_ctl_t node_reference;
};

struct ek3_session {
    int fd;
    int status;
    char id[EK3_SESSION_ID_STRLEN];
    list_ctl_t events_notified, events_registered, events_deployed;
    list_ctl_t node_reference;
    struct scraft_hashkey hash_reference;
    struct ek3_reactor *parent_reactor;
};

#define     EK3_SESSION_NOT_ACTIVATED       0
#define     EK3_SESSION_ACTIVATED           1
#define     EK3_SESSION_COMMITTED           2
#define     EK3_SESSIONFLAG_POSTED          16

#define     EK3_SESSIONFLAG_ALL             (EK3_SESSIONFLAG_POSTED)

#endif      // EK3_REACTOR_H_
