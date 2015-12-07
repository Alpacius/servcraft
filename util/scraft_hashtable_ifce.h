#ifndef     SCRAFT_HASHTABLE_H_
#define     SCRAFT_HASHTABLE_H_

#include    "./scraft_hashtable.h"
#include    "../include/miscutils.h"


struct scraft_hashtable *scraft_hashtable_new(
        struct scraft_model_alloc allocator, 
        uint64_t cap, 
        int (*key_compare)(struct scraft_hashkey *, struct scraft_hashkey *),
        int (*key_destroy)(struct scraft_hashkey *),
        uint64_t (*hashfunc)(struct scraft_hashkey *));
void scraft_hashtable_destroy(struct scraft_hashtable *dic);
struct scraft_hashtable *scraft_hashtable_insert(struct scraft_hashtable *dic, struct scraft_hashkey *key);
struct scraft_hashkey *scraft_hashtable_fetch(struct scraft_hashtable *dic, struct scraft_hashkey *key);
void scraft_hashtable_delete(struct scraft_hashtable *dic, struct scraft_hashkey *key);
struct scraft_hashkey *scraft_hashtable_remove(struct scraft_hashtable *dic, struct scraft_hashkey *key);
uint64_t scraft_hashaux_djb_cstring(const char *cstr);


#endif      // SCRAFT_HASHTABLE_H_
