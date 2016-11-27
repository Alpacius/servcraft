#include    "./p7r_stack_hint_dictionary.h"
#include    "./p7r_root_alloc.h"


struct p7r_stack_hint_dictionary *p7r_stack_hint_dictionary_init(
        struct p7r_stack_hint_dictionary *dictionary, 
        uint64_t capacity, 
        struct p7r_stack_allocator *stack_allocator
) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    if ((dictionary->hashtable = scraft_hashtable_new(allocator, capacity, p7r_stack_hint_entry_compare, p7r_stack_hint_entry_destroy, p7r_stack_hint_entry_hash)) == NULL)
        return NULL;
    dictionary->allocator = stack_allocator;
    return dictionary;
}

void p7r_stack_hint_dictionary_ruin(struct p7r_stack_hint_dictionary *dictionary) {
    scraft_hashtable_destroy(dictionary->hashtable);
}

int p7r_stack_hint_dictionary_put(struct p7r_stack_hint_dictionary *dictionary, struct p7r_stack_hint *hint) {
    return scraft_hashtable_insert(dictionary->hashtable, &(hint->hashable)) != NULL;
}

static inline
struct p7r_stack_hint *do_fetch_hint(struct p7r_stack_hint_dictionary *dictionary, struct p7r_stack_hint *hint_dummy) {
    struct scraft_hashkey *target_key = scraft_hashtable_fetch(dictionary->hashtable, &(hint_dummy->hashable));
    if (unlikely(target_key == NULL))
        return NULL;
    return container_of(target_key, struct p7r_stack_hint, hashable);
}

struct p7r_stack_hint *p7r_stack_hint_dictionary_get_by_name(struct p7r_stack_hint_dictionary *dictionary, const char *name) {
    struct p7r_stack_hint hint_dummy = { .key = { .name = name, .solid_entrance = NULL } };
    return do_fetch_hint(dictionary, &hint_dummy);
}

struct p7r_stack_hint *p7r_stack_hint_dictionary_get_by_entrance(struct p7r_stack_hint_dictionary *dictionary, void (*entrance)(void *)) {
    struct p7r_stack_hint hint_dummy = { .key = { .name = NULL, .solid_entrance = entrance } };
    return do_fetch_hint(dictionary, &hint_dummy);
}

static inline
void do_delete_hint(struct p7r_stack_hint_dictionary *dictionary, struct p7r_stack_hint *hint_dummy) {
    scraft_hashtable_delete(dictionary->hashtable, &(hint_dummy->hashable));
}

void p7r_stack_hint_dictionary_delete_by_name(struct p7r_stack_hint_dictionary *dictionary, const char *name) {
    struct p7r_stack_hint hint_dummy = { .key = { .name = name, .solid_entrance = NULL } };
    do_delete_hint(dictionary, &hint_dummy);
}

void p7r_stack_hint_dictionary_delete_by_entrance(struct p7r_stack_hint_dictionary *dictionary, void (*entrance)(void *)) {
    struct p7r_stack_hint hint_dummy = { .key = { .name = NULL, .solid_entrance = entrance } };
    do_delete_hint(dictionary, &hint_dummy);
}
