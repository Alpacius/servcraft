#ifndef     P7_NAMESPACE_H_
#define     P7_NAMESPACE_H_

#include    <stddef.h>
#include    <stdint.h>

#include    "./p7_root_alloc.h"
#include    "../include/model_alloc.h"
#include    "../util/scraft_hashtable_ifce.h"

struct p7_namebind {
    struct scraft_hashkey keyctl;
    void *coro;
    uint32_t namelen;
    const char *name_const;
    char name[];
};

void *p7_name_register(void *coro, const char *name);
void p7_name_discard(void *name_);
void *p7_namespace_find(const char *name);
int p7_namespace_init(uint64_t cap);

#endif      // P7_NAMESPACE_H_
