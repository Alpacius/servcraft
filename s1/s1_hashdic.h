#ifndef     S1_HASHDIC_H_
#define     S1_HASHDIC_H_

#include    <stdint.h>
#include    "../include/util_list.h"

struct s1_dic_entry {
    uint32_t orig_hashval;
    void *key, *value;
    list_ctl_t lctl;
};

struct s1_dic {
    uint32_t size, cap;
    uint32_t (*hasher)(const void *);
    void (*key_dtor)(void *);
    void (*val_dtor)(void *);
    int (*equal_to)(const void *, const void *);
    struct s1_dic_entry **dic;
    struct s1_dic_entry *dummies;
};

struct s1_dic *s1_dic_init(uint32_t (*hasher)(const void *), void (*key_dtor)(void *), void (*val_dtor)(void *), int (*equal_to)(const void *, const void *), uint32_t cap);
void s1_dic_ruin(struct s1_dic *dic);
int s1_dic_insert(struct s1_dic *dic, void *key, void *value);
int s1_dic_delete(struct s1_dic *dic, const void *key);
void *s1_dic_fetch(struct s1_dic *dic, const void *key);
void s1_dic_rehash(struct s1_dic *dic);

uint32_t s1_hasher_cstring_djb(const void *key);

#endif      //  S1_HASHDIC_H_
