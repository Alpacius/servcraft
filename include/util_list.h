// using GCC extensions

#ifndef        _UTIL_LIST_H_
#define        _UTIL_LIST_H_

#include    <stddef.h>
#include    "./miscutils.h"


// circle linked list

struct list_ctl_struct {
    struct list_ctl_struct *prev, *next;
};
typedef struct list_ctl_struct list_ctl_t;

static inline void init_list_head(list_ctl_t *h) {
    h->next = h;
    h->prev = h;
}

static inline void __list_add(list_ctl_t *e, list_ctl_t *p, list_ctl_t *n) {
    n->prev = e;
    e->next = n;
    e->prev = p;
    p->next = e;
}

static inline void list_add_head(list_ctl_t *e, list_ctl_t *h) {
    __list_add(e, h, h->next);
}

static inline void list_add_tail(list_ctl_t *e, list_ctl_t *h) {
    __list_add(e, h->prev, h);
}

static inline void __list_del(list_ctl_t *p, list_ctl_t *n) {
    n->prev = p;
    p->next = n;
}

static inline void list_del(list_ctl_t *e) {
    __list_del(e->prev, e->next);
    e->next = e->prev = NULL;
}

static inline int list_node_isolated(list_ctl_t *e) {
    return (e->next == NULL) || (e->prev == NULL);
}

static inline void list_node_isolate(list_ctl_t *e) {
    e->prev = e->next = NULL;
}

static inline int list_is_empty(list_ctl_t *h) {
    return (h->next == h) && (h->prev == h);
}

#define    list_entry(ptr, type, member)    container_of(ptr, type, member)

#define    list_foreach(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

#define    list_foreach_remove(pos, head, tmp) \
    for (pos = (head)->next; \
        ((tmp = pos) != (head)) && (pos = pos->next);)

#endif        // _UTIL_LIST_H_
