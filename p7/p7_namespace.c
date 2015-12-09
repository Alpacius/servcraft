#include    <string.h>
#include    "./p7_namespace.h"

#define namebind_actual_key(x_) ({ __auto_type x__ = (x_); (x__->name_const != NULL) ? x__->name_const : x__->name; })

static struct scraft_hashtable *p7_global_namespace = NULL;


static inline
struct p7_namebind *p7_namebind_new(const char *name, void *coro) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    uint32_t namelen = strlen(name);
    struct p7_namebind *namerec = scraft_allocate(allocator, sizeof(struct p7_namebind) + sizeof(char) * namelen);
    if (likely(namerec != NULL)) {
        (namerec->namelen = namelen), (namerec->coro = coro), (namerec->name_const = NULL);
        strcpy(namerec->name, name);
    }
    return namerec;
}

void *p7_name_register(void *coro, const char *name) {
    struct p7_namebind *namerec = p7_namebind_new(name, coro);
    if (likely(namerec != NULL))
        scraft_hashtable_insert(p7_global_namespace, &(namerec->keyctl));
    return namerec;
}

void p7_name_discard(void *name_) {
    struct p7_namebind *name = name_;
    scraft_hashtable_delete(p7_global_namespace, &(name->keyctl));
}

void *p7_namespace_find(const char *name) {
    struct p7_namebind *query = __builtin_alloca(sizeof(struct p7_namebind));
    query->name_const = name;
    struct scraft_hashkey *result = scraft_hashtable_fetch(p7_global_namespace, &(query->keyctl));
    return (result != NULL) ? container_of(result, struct p7_namebind, keyctl)->coro : NULL;
}

static
int p7_namebind_compare(struct scraft_hashkey *lhs_, struct scraft_hashkey *rhs_) {
    struct p7_namebind *lhs = container_of(lhs_, struct p7_namebind, keyctl), *rhs = container_of(rhs_, struct p7_namebind, keyctl);
    return strcmp(namebind_actual_key(lhs), namebind_actual_key(rhs));
}

static
int p7_namebind_destroy(struct scraft_hashkey *arg) {
    struct p7_namebind *namerec = container_of(arg, struct p7_namebind, keyctl);
    __auto_type allocator = p7_root_alloc_get_proxy();
    scraft_deallocate(allocator, namerec);
    return 0;
}

static
uint64_t p7_namebind_hashfunc(struct scraft_hashkey *arg) {
    struct p7_namebind *namerec = container_of(arg, struct p7_namebind, keyctl);
    return scraft_hashaux_djb_cstring(namebind_actual_key(namerec));
}

int p7_namespace_init(uint64_t cap) {
    return ((p7_global_namespace = scraft_hashtable_new(p7_root_alloc_get_proxy(), cap, p7_namebind_compare, p7_namebind_destroy, p7_namebind_hashfunc)) != NULL) ? 0 : -1;
}
