#include    <stdio.h>
#include    "./util_heap.h"
#include    "./p7_root_alloc.h"
#include    "../include/model_alloc.h"


// note that we use entries[1] as the root

struct p7_minheap *heap_create(compare_func_t cmp_func) {
    __auto_type allocator = local_root_alloc_get_proxy();
    struct p7_minheap *h = scraft_allocate(allocator, sizeof(struct p7_minheap));
    //struct p7_minheap *h = (struct p7_minheap *) malloc(sizeof(struct p7_minheap));
    if (h == NULL)
        return NULL;
    //h->heap_entries = (void **) malloc(sizeof(void *) * HEAP_INIT_SIZE);
    h->heap_entries = scraft_allocate(allocator, sizeof(void *) * HEAP_INIT_SIZE);
    if (h->heap_entries == NULL) {
        scraft_deallocate(allocator, h);
        //free(h);
        return NULL;
    }
    h->heap_size = HEAP_INIT_SIZE;
    h->heap_curr = 0;
    h->heap_compare = cmp_func;
    memset(h->heap_entries, sizeof(void *) * HEAP_INIT_SIZE, 0);
    return h;
}


static
void min_heapify(unsigned i, struct p7_minheap *h) {
    unsigned l = 2 * i, r = 2 * i + 1, small;
    void *tmp_entry;
    if ((l <= h->heap_curr) && (h->heap_compare(h->heap_entries[l], h->heap_entries[i]) == -1))
        small = l;
    else
        small = i;
    if ((r <= h->heap_curr) && (h->heap_compare(h->heap_entries[r], h->heap_entries[small]) == -1))
        small = r;
    if (small != i) {
        tmp_entry = h->heap_entries[i];
        h->heap_entries[i] = h->heap_entries[small];
        h->heap_entries[small] = tmp_entry;
        min_heapify(small, h);
    }
}

void *heap_peek_min(struct p7_minheap *h) {
    return (h->heap_curr > 0) ? h->heap_entries[1] : NULL;
}

void *heap_extract_min(struct p7_minheap *h) {
    void *min_elt = h->heap_entries[1];
    if (h->heap_curr == 0)
        return NULL;
    h->heap_entries[1] = h->heap_entries[(h->heap_curr)--];
    min_heapify(1, h);
    return min_elt;
}

void heap_insert(void *e, struct p7_minheap *h) {
    unsigned j;
    void *tmp_entry;
    if (h->heap_curr >= h->heap_size) {
        __auto_type basesize = sizeof(void *) * h->heap_size;
        __auto_type allocator = local_root_alloc_get_proxy();
        void **tmp_entries = (void **) scraft_reallocate(allocator, h->heap_entries, basesize * 2);
        if (tmp_entries != NULL) {
            h->heap_entries = tmp_entries;
            h->heap_size *= 2;
        } else {
            return;
        }
    }
    h->heap_entries[++(h->heap_curr)] = e;
    for (j = h->heap_curr; j > 1 && h->heap_compare(h->heap_entries[j/2], h->heap_entries[j]) == 1; j /= 2) {
        tmp_entry = h->heap_entries[j];
        h->heap_entries[j] = h->heap_entries[j/2];
        h->heap_entries[j/2] = tmp_entry;
    }
}

void heap_delete(void *e, struct p7_minheap *h) {
    if (h->heap_curr == 0)
        return;
    unsigned j;
    void *tmp_entry;
    int retry = 1;
    // XXX no error check - use with care
    unsigned idx;
    for (idx = 1; idx <= h->heap_curr; idx++)
        if (e == h->heap_entries[idx])
            break;
    if (idx > h->heap_curr)
        return;
    h->heap_entries[idx] = h->heap_entries[h->heap_curr];
    (h->heap_curr)--;
    min_heapify(idx, h);
}
