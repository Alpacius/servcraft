#ifndef     EK3_REACTOR_IFCE_H_
#define     EK3_REACTOR_IFCE_H_

#include    "./ek3_reactor.h"

struct ek3_event *ek3_event_create(int type, int fd, void *(*handler)(struct ek3_event *, void *), void (*param_dtor)(void *), void *handler_user_param, ...);
void ek3_event_destroy(struct ek3_event *event);

struct ek3_session *ek3_session_create_v4(int fd, struct sockaddr_in *client_address);
struct ek3_session *ek3_session_create_v6(int fd, struct sockaddr_in6 *client_address);
void ek3_session_destroy(struct ek3_session *session);
void ek3_session_detach(struct ek3_session *session);
void ek3_session_register_event(struct ek3_session *session, struct ek3_event *event);
struct ek3_event *ek3_session_next_event_notified(struct ek3_session *session);

struct ek3_reactor *ek3_reactor_create(uint32_t epoll_capacity, uint64_t hash_capacity);
void ek3_reactor_destroy(struct ek3_reactor *reactor);

int ek3_commit_session(struct ek3_reactor *reactor, struct ek3_session *session);
void ek3_register_session(struct ek3_reactor *reactor, struct ek3_session *session);
struct ek3_session *ek3_poll(struct ek3_reactor *reactor);

#endif      // EK3_REACTOR_IFCE_H_
