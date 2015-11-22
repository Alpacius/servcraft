#ifndef     SCRAFT_ARENA_H_
#define     SCRAFT_ARENA_H_

#include    "../include/miscutils.h"
#include    "../include/util_list.h"
#include    "../include/model_alloc.h"
#include    <stdint.h>
#include    <stddef.h>

struct scraft_arena_large {
    list_ctl_t lctl;
    char block[];
};

struct scraft_arena_elt {
    uint32_t size, pos;
    uint32_t failed, nfail;
    list_ctl_t lctl;
    char pool[];
};

struct scraft_arena_dtor_hook {
    void *user_arg, *this_ptr;
    void (*dtor)(void *, void *);
    list_ctl_t lctl;
};

struct scraft_arena_dtor_chain {
    uint32_t nbase, ncurrent;
    list_ctl_t extra_hooks;
    struct scraft_arena_dtor_hook base_hooks[];
};

struct scraft_arena {
    uint32_t nlarge, basesize, nfail;
    list_ctl_t arena_set, large_block_set, deprecated_set;
    struct scraft_arena_dtor_chain *dtor_chain;
    struct scraft_model_alloc allocator;
};

struct scraft_arena_response {
    void *object;
    int success;
};

#endif      // SCRAFT_ARENA_H_
