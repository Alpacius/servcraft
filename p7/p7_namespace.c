#include    <string.h>
#include    "./p7_namespace.h"
#include    "./rwspin.h"

#define namebind_actual_key(x_) ({ __auto_type x__ = (x_); (x__->name_const != NULL) ? x__->name_const : x__->name; })

static struct scraft_hashtable *p7_global_namespace = NULL;

static struct p7_global_namespace_guard {
    uint64_t size, granularity;
    struct p7_rwspinlock locks[];
} *guard = NULL;
static uint64_t namespace_size = 0;

static uint64_t p7_namebind_hashfunc(struct scraft_hashkey *arg);

int p7_namespace_guard_init(uint64_t granularity, uint32_t spintime) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    if ((guard = scraft_allocate(allocator, sizeof(struct p7_global_namespace_guard) + sizeof(struct p7_rwspinlock) * (namespace_size / granularity + (namespace_size % granularity != 0)))) == NULL)
        return 0;
    (guard->size = namespace_size / granularity + (namespace_size % granularity != 0)), (guard->granularity = granularity);
    uint64_t idx;
    for (idx = 0; idx < guard->size; idx++)
        p7_rwspinlock_init(&(guard->locks[idx]), spintime);
    return 1;
}

static inline
struct p7_namebind *p7_namebind_new(const char *name, void *coro) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    uint32_t namelen = strlen(name) + 1;
    struct p7_namebind *namerec = scraft_allocate(allocator, sizeof(struct p7_namebind) + sizeof(char) * namelen);
    if (likely(namerec != NULL)) {
        (namerec->namelen = namelen), (namerec->coro = coro), (namerec->name_const = NULL);
        strcpy(namerec->name, name);
    }
    return namerec;
}

void *p7_namespace_find(const char *name);

void *p7_name_register(void *coro, const char *name) {
    if (p7_namespace_find(name))
        return NULL;
    struct p7_namebind *namerec = p7_namebind_new(name, coro);
    if (likely(namerec != NULL)) {
        uint64_t lockidx = (p7_namebind_hashfunc(&(namerec->keyctl)) % namespace_size) / guard->granularity;
        p7_rwspinlock_wrlock(&(guard->locks[lockidx]));
        scraft_hashtable_insert(p7_global_namespace, &(namerec->keyctl));
        p7_rwspinlock_wrunlock(&(guard->locks[lockidx]));
    }
    return namerec;
}

void p7_name_discard(void *name_) {
    struct p7_namebind *name = name_;
    uint64_t lockidx = (p7_namebind_hashfunc(&(name->keyctl)) % namespace_size) / guard->granularity;
    p7_rwspinlock_wrlock(&(guard->locks[lockidx]));
    scraft_hashtable_delete(p7_global_namespace, &(name->keyctl));
    p7_rwspinlock_wrunlock(&(guard->locks[lockidx]));
}

void *p7_namespace_find(const char *name) {
    struct p7_namebind *query = __builtin_alloca(sizeof(struct p7_namebind));
    query->name_const = name;
    uint64_t lockidx = (p7_namebind_hashfunc(&(query->keyctl)) % namespace_size) / guard->granularity;
    p7_rwspinlock_rdlock(&(guard->locks[lockidx]));
    struct scraft_hashkey *result = scraft_hashtable_fetch(p7_global_namespace, &(query->keyctl));
    p7_rwspinlock_rdunlock(&(guard->locks[lockidx]));
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
    return ((p7_global_namespace = scraft_hashtable_new(p7_root_alloc_get_proxy(), (namespace_size = cap), p7_namebind_compare, p7_namebind_destroy, p7_namebind_hashfunc)) != NULL) ? 1 : 0;
}
