#include    <stddef.h>
#include    <stdint.h>
#include    <string.h>
#include    "./s1_root_alloc.h"
#include    "./s1_hashdic.h"


struct s1_dic *s1_dic_init(uint32_t (*hasher)(const void *), void (*key_dtor)(void *), void (*val_dtor)(void *), int (*equal_to)(const void *, const void *), uint32_t cap) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    struct s1_dic *dic = scraft_allocate(allocator, sizeof(struct s1_dic));
    (dic->hasher = hasher), (dic->size = 0), (dic->cap = cap), (dic->key_dtor = key_dtor), (dic->val_dtor = val_dtor), (dic->equal_to = equal_to);
    dic->dic = scraft_allocate(allocator, sizeof(struct s1_dic_entry *) * cap);
    dic->dummies = scraft_allocate(allocator, sizeof(struct s1_dic_entry) * cap);
    for (uint32_t idx = 0; idx < cap; idx++) {
        dic->dic[idx] = &dic->dummies[idx];
        init_list_head(&(dic->dummies[idx].lctl));
        dic->dummies[idx].value = dic->dummies[idx].key = NULL;
    }
    return dic;
}

void s1_dic_ruin(struct s1_dic *dic) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    for (uint32_t idx = 0; idx < dic->cap; idx++) {
        struct s1_dic_entry *slot = dic->dic[idx];
        list_ctl_t *p, *t, *h = &(slot->lctl);
        list_foreach_remove(p, h, t) {
            list_del(t);
            struct s1_dic_entry *item = container_of(t, struct s1_dic_entry, lctl);
            if (dic->key_dtor != NULL) dic->key_dtor(item->key);
            if (dic->key_dtor != NULL) dic->val_dtor(item->value);
            scraft_deallocate(allocator, item);
        }
    }
    scraft_deallocate(allocator, dic->dummies);
    scraft_deallocate(allocator, dic->dic);
    scraft_deallocate(allocator, dic);
}

void s1_dic_rehash(struct s1_dic *dic) {
    // TODO implementation
}

static
int dic_insert_at(struct s1_dic *dic, uint32_t idx, void *key, void *value) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    struct s1_dic_entry *slot = dic->dic[idx % dic->cap];
    struct s1_dic_entry *item = scraft_allocate(allocator, sizeof(struct s1_dic_entry));
    if (slot != NULL) {
        dic->size++;
        (item->orig_hashval = idx), (item->key = key), (item->value = value);
        list_add_tail(&(item->lctl), &(slot->lctl));
        return 0;
    } else
        return -1;
}

int s1_dic_insert(struct s1_dic *dic, void *key, void *value) {
    return dic_insert_at(dic, dic->hasher(key), key, value);
}

int s1_dic_delete(struct s1_dic *dic, const void *key) {
    uint32_t idx = dic->hasher(key);
    struct s1_dic_entry *slot = dic->dic[idx % dic->cap];
    list_ctl_t *p, *t, *h = &(slot->lctl);
    list_foreach_remove(p, h, t) {
        struct s1_dic_entry *item = container_of(t, struct s1_dic_entry, lctl);
        if (dic->equal_to(key, item->key) == 0) {
            list_del(&(item->lctl));
            dic->size--;
            if (dic->key_dtor != NULL) dic->key_dtor(item->key);
            if (dic->val_dtor != NULL) dic->val_dtor(item->value);
            __auto_type allocator = s1_root_alloc_get_proxy();
            scraft_deallocate(allocator, item);
            return 0;
        }
    }
    return -1;
}

void *s1_dic_fetch(struct s1_dic *dic, const void *key) {
    uint32_t idx = dic->hasher(key);
    struct s1_dic_entry *slot = dic->dic[idx % dic->cap];
    list_ctl_t *p, *h = &(slot->lctl);
    void *ret = NULL;
    list_foreach(p, h) {
        struct s1_dic_entry *item = container_of(p, struct s1_dic_entry, lctl);
        if (dic->equal_to(key, item->key) == 0) {
            ret = item->value;
            break;
        }
    }
    return ret;
}

uint32_t s1_hasher_cstring_djb(const void *key) {
    uint32_t hashval = 5381;
    const char *string = (const char *) key;
    for (uint32_t idx = 0; string[idx]; idx++)
        hashval = ( (hashval << 5) + hashval ) + string[idx];
    return hashval;
}
