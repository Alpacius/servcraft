#ifndef     SCRAFT_LRU_CACHE_H_
#define     SCRAFT_LRU_CACHE_H_

#include    <stddef.h>
#include    <stdint.h>
#include    "../include/util_list.h"
#include    "./scraft_rbt_ifce.h"

struct scraft_lru_cache_entry {
    void *key_ref;
    struct scraft_rbtree_node rbtctl;
    list_ctl_t lctl;
};

struct scraft_lru_cache {
    uint32_t cap, size;
    struct scraft_rbtree map;
    list_ctl_t queue;
    int (*key_compare_forward)(const void *, const void *);
    void (*dtor)(void *);
};

#endif      // SCRAFT_LRU_CACHE_H_
