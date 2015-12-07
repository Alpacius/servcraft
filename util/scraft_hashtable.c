#include    "../include/miscutils.h"
#include    "./scraft_hashtable.h"


struct scraft_hashtable *scraft_hashtable_new(
        struct scraft_model_alloc allocator, 
        uint64_t cap, 
        int (*key_compare)(struct scraft_hashkey *, struct scraft_hashkey *),
        int (*key_destroy)(struct scraft_hashkey *),
        uint64_t (*hashfunc)(struct scraft_hashkey *)) {
    struct scraft_hashtable *dic = scraft_allocate(allocator, sizeof(struct scraft_hashtable));
    if (unlikely(dic == NULL))
        return NULL;
    if (unlikely((dic->slots = scraft_allocate(allocator, sizeof(struct scraft_hashslot) * cap)) == NULL)) {
        scraft_deallocate(allocator, dic);
        return NULL;
    }
    (dic->allocator = allocator), (dic->key_compare = key_compare), (dic->key_destroy = key_destroy), (dic->hashfunc = hashfunc), (dic->cap = cap);
    uint64_t idx;
    for (idx = 0; idx < cap; idx++) {
        dic->slots[idx].size = 0;
        init_list_head(&(dic->slots[idx].cluster));
    }
    return dic;
}

void scraft_hashtable_destroy(struct scraft_hashtable *dic) {
    uint64_t idx;
    for (idx = 0; idx < dic->cap; idx++) {
        list_ctl_t *p, *t;
        list_foreach_remove(p, &(dic->slots[idx].cluster), t) {
            list_del(t);
            dic->slots[idx].size--;
            if (dic->key_destroy != NULL)
                dic->key_destroy(container_of(t, struct scraft_hashkey, lctl));
        }
    }
    scraft_deallocate(dic->allocator, dic->slots);
    scraft_deallocate(dic->allocator, dic);
}

struct scraft_hashtable *scraft_hashtable_insert(struct scraft_hashtable *dic, struct scraft_hashkey *key) {
    uint64_t hashval = dic->hashfunc(key) % dic->cap;
    dic->slots[hashval].size++;
    list_add_head(&(key->lctl), &(dic->slots[hashval].cluster));
    key->n_hits = 0;
    return dic;
}

static inline
struct scraft_hashkey *scraft_hashtable_find(struct scraft_hashtable *dic, uint64_t hashval, struct scraft_hashkey *key) {
    list_ctl_t *p;
    list_foreach(p, &(dic->slots[hashval].cluster))
        if (dic->key_compare(key, container_of(p, struct scraft_hashkey, lctl)) == 0)
            return container_of(p, struct scraft_hashkey, lctl);
    return NULL;
}

struct scraft_hashkey *scraft_hashtable_fetch(struct scraft_hashtable *dic, struct scraft_hashkey *key) {
    uint64_t hashval = dic->hashfunc(key) % dic->cap;
    struct scraft_hashkey *key_actual = scraft_hashtable_find(dic, hashval, key);
    if (unlikely(key_actual == NULL))
        return NULL;
    list_del(&(key_actual->lctl));
    list_add_head(&(key_actual->lctl), &(dic->slots[hashval].cluster));
    key_actual->n_hits++;
    return key_actual;
}

static inline
struct scraft_hashkey *scraft_hashtable_remove_inner(struct scraft_hashtable *dic, struct scraft_hashkey *key) {
    uint64_t hashval = dic->hashfunc(key) % dic->cap;
    struct scraft_hashkey *key_actual = scraft_hashtable_find(dic, hashval, key);
    if (unlikely(key_actual == NULL))
        return NULL;
    list_del(&(key_actual->lctl));
    return key_actual;
}

void scraft_hashtable_delete(struct scraft_hashtable *dic, struct scraft_hashkey *key) {
    struct scraft_hashkey *key_actual = scraft_hashtable_remove_inner(dic, key);
    if (likely(dic->key_destroy != NULL))
        dic->key_destroy(key_actual);
}

struct scraft_hashkey *scraft_hashtable_remove(struct scraft_hashtable *dic, struct scraft_hashkey *key) {
    return scraft_hashtable_remove_inner(dic, key);
}

uint64_t scraft_hashaux_djb_cstring(const char *cstr) {
    uint64_t hashval = 5381;
    uint64_t idx;
    for (idx = 0; cstr[idx]; idx++)
        hashval = ((hashval << 5) + hashval) + cstr[idx];
    return hashval;
}
