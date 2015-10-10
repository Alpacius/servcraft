#ifndef     _UTIL_HEAP_H_
#define     _UTIL_HEAP_H_

#include    <stdlib.h>
#include    <string.h>

#define     HEAP_INIT_SIZE      1024
#define     HEAP_MAX_SIZE       ((HEAP_INIT_SIZE) * (1<<6))

typedef int (*compare_func_t)(const void *, const void *);

struct p7_minheap {
    void **heap_entries;
    unsigned heap_size, heap_curr;
    compare_func_t heap_compare;
};

struct p7_minheap *heap_create(compare_func_t cmp_func);
void *heap_peek_min(struct p7_minheap *h);
void *heap_extract_min(struct p7_minheap *h);
void heap_insert(void *e, struct p7_minheap *h);
void heap_delete(void *e, struct p7_minheap *h);

#endif      // _UTIL_HEAP_H_
