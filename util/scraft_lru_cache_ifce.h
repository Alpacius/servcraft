#ifndef     SCRAFT_LRU_CACHE_IFCE_H_
#define     SCRAFT_LRU_CACHE_IFCE_H_

#include    "./scraft_lru_cache.h"

void scraft_lru_cache_init(struct scraft_lru_cache *cache, uint32_t cap, int (*key_compare_forward)(const void *, const void *), void (*dtor)(void *));
void scraft_lru_cache_delete(struct scraft_lru_cache *cache, struct scraft_lru_cache_entry *entry);
void scraft_lru_cache_add(struct scraft_lru_cache *cache, struct scraft_lru_cache_entry *entry);
struct scraft_lru_cache_entry *scraft_lru_cache_fetch(struct scraft_lru_cache *cache, const void *key);

#endif      // SCRAFT_LRU_CACHE_IFCE_H_
