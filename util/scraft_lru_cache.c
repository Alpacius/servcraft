#include    "./scraft_lru_cache.h"


void scraft_lru_cache_init(struct scraft_lru_cache *cache, uint32_t cap, int (*key_compare_forward)(const void *, const void *), void (*dtor)(void *)) {
    (cache->size = 0), (cache->cap = cap), (cache->key_compare_forward = key_compare_forward), (cache->dtor = dtor);
    init_list_head(&(cache->queue));
    scraft_rbt_init(&(cache->map), cache->key_compare_forward);
}

static inline
void scraft_lru_cache_add_(struct scraft_lru_cache *cache, struct scraft_lru_cache_entry *entry) {
    entry->rbtctl.key_ref = entry->key_ref;
    list_add_head(&(entry->lctl), &(cache->queue));
    scraft_rbt_insert(&(cache->map), &(entry->rbtctl));
    cache->size++;
}

void scraft_lru_cache_delete(struct scraft_lru_cache *cache, struct scraft_lru_cache_entry *entry) {
    list_del(&(entry->lctl));
    scraft_rbt_delete(&(cache->map), &(entry->rbtctl));
    cache->dtor(entry);
    cache->size--;
}

void scraft_lru_cache_add(struct scraft_lru_cache *cache, struct scraft_lru_cache_entry *entry) {
    scraft_lru_cache_add_(cache, entry);
    if (cache->size > cache->cap)
        scraft_lru_cache_delete(cache, container_of(cache->queue.prev, struct scraft_lru_cache_entry, lctl));
}

struct scraft_lru_cache_entry *scraft_lru_cache_fetch(struct scraft_lru_cache *cache, const void *key) {
    struct scraft_rbtree_node *query = scraft_rbt_find(&(cache->map), key);
    struct scraft_lru_cache_entry *ret = NULL;
    if (query != NULL) {
        ret = container_of(query, struct scraft_lru_cache_entry, rbtctl);
        list_del(&(ret->lctl));
        list_add_head(&(ret->lctl), &(cache->queue));
    }
    return ret;
}
