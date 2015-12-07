#ifndef     SCRAFT_HASHTABLE_
#define     SCRAFT_HASHTABLE_

#include    <stddef.h>
#include    <stdint.h>
#include    "../include/util_list.h"
#include    "../include/model_alloc.h"

struct scraft_hashkey {
    uint64_t n_hits;
    list_ctl_t lctl;
};

struct scraft_hashslot {
    list_ctl_t cluster;
    uint64_t size;
};

struct scraft_hashtable {
    struct scraft_model_alloc allocator;
    uint64_t cap;
    int (*key_compare)(struct scraft_hashkey *, struct scraft_hashkey *);
    int (*key_destroy)(struct scraft_hashkey *);
    uint64_t (*hashfunc)(struct scraft_hashkey *);
    struct scraft_hashslot *slots;
};

#endif      // SCRAFT_HASHTABLE
