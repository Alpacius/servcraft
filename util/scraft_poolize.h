#ifndef     SCRAFT_POOLIZE_H_
#define     SCRAFT_POOLIZE_H_

#include    <stddef.h>
#include    <stdint.h>
#include    "../include/util_list.h"

#define     POOL_PUT_SUCCESS         0
#define     POOL_PUT_FAIL_FULL      -1

struct scraft_object_pool {
    uint32_t cap, size;
    list_ctl_t pool;
};

static inline
int scraft_pool_put(struct scraft_object_pool *pool, list_ctl_t *elt) {
    int ret = POOL_PUT_FAIL_FULL;
    (pool->size < pool->cap) && ((pool->size++), (list_add_tail(elt, &(pool->pool))), (ret = POOL_PUT_SUCCESS));
    return ret;
}

static inline
list_ctl_t *scraft_pool_get(struct scraft_object_pool *pool) {
    list_ctl_t *elt = NULL;
    return (pool->size) ? ((pool->size--), (elt = pool->pool.next), (list_del(elt)), elt) : NULL;
}

static inline
void scraft_pool_init(struct scraft_object_pool *pool, uint32_t cap) {
    (pool->cap = cap), (pool->size = 0);
    init_list_head(&(pool->pool));
}

#endif      // SCRAFT_POOLIZE_H_
