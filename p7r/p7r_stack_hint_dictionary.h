#ifndef     P7R_STACK_HINT_DICTIONARY_H_
#define     P7R_STACK_HINT_DICTIONARY_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"
#include    "./p7r_stack_hint.h"


struct p7r_stack_hint_dictionary {
    struct scraft_hashtable *hashtable;
    struct p7r_stack_allocator *allocator;
};

struct p7r_stack_hint_dictionary *p7r_stack_hint_dictionary_init(
        struct p7r_stack_hint_dictionary *dictionary, 
        uint64_t capacity, 
        struct p7r_stack_allocator *stack_allocator
);
void p7r_stack_hint_dictionary_ruin(struct p7r_stack_hint_dictionary *dictionary);

int p7r_stack_hint_dictionary_put(struct p7r_stack_hint_dictionary *dictionary, struct p7r_stack_hint *hint);

struct p7r_stack_hint *p7r_stack_hint_dictionary_get_by_name(struct p7r_stack_hint_dictionary *dictionary, const char *name);
struct p7r_stack_hint *p7r_stack_hint_dictionary_get_by_entrance(struct p7r_stack_hint_dictionary *dictionary, void (*entrance)(void *));

#define p7r_stack_hint_dictionary_get(dictionary_, arg_) \
    _Generic((arg_), char *: p7r_stack_hint_dictionary_get_by_name, void (*)(void *): p7r_stack_hint_dictionary_get_by_entrance)((dictionary_), (arg_))

void p7r_stack_hint_dictionary_delete_by_name(struct p7r_stack_hint_dictionary *dictionary, const char *name);
void p7r_stack_hint_dictionary_delete_by_entrance(struct p7r_stack_hint_dictionary *dictionary, void (*entrance)(void *));

#define p7r_stack_hint_dictionary_delete(dictionary_, arg_) \
    _Generic((arg_), char *: p7r_stack_hint_dictionary_delete_by_name, void (*)(void *): p7r_stack_hint_dictionary_delete_by_entrance)((dictionary_), (arg_))

#endif      // P7R_STACK_HINT_DICTIONARY_H_
