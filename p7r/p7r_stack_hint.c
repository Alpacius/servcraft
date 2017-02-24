#include    "./p7r_stack_hint.h"
#include    "./p7r_root_alloc.h"


static inline
void config_as_base_of(struct p7r_stack_hint *hint, const struct p7r_stack_hint_config *config) {
    hint->execution_time_stat = config->execution_time_stat;
    hint->failure_stat = config->failure_stat;
}

static inline
void init_policy(struct p7r_stack_hint *hint) {
    hint->policy = P7R_STACK_POLICY_DEFAULT;
}

struct p7r_stack_hint *p7r_stack_hint_init_by_name(struct p7r_stack_hint *hint, const char *name, const struct p7r_stack_hint_config *config) {
    (hint->key.solid_entrance = NULL), (hint->key.name = name);
    init_policy(hint);
    config_as_base_of(hint, config);
    return hint;
}

struct p7r_stack_hint *p7r_stack_hint_init_by_entrance(struct p7r_stack_hint *hint, void (*entrance)(void *), const struct p7r_stack_hint_config *config) {
    (hint->key.solid_entrance = entrance), (hint->key.name = NULL);
    init_policy(hint);
    config_as_base_of(hint, config);
    return hint;
}

static inline
struct p7r_stack_hint *p7r_stack_hint_new_blank() {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    struct p7r_stack_hint *hint = scraft_allocate(allocator, sizeof(struct p7r_stack_hint));
    if (unlikely(hint == NULL))
        return NULL;
    return hint;
}

struct p7r_stack_hint *p7r_stack_hint_new_from_name(const char *name, struct p7r_stack_hint_config config) {
    struct p7r_stack_hint *hint = p7r_stack_hint_new_blank();
    if (unlikely(hint == NULL))
        return NULL;
    return p7r_stack_hint_init_by_name(hint, name, &config);
}

struct p7r_stack_hint *p7r_stack_hint_new_from_entrance(void (*entrance)(void *), struct p7r_stack_hint_config config) {
    struct p7r_stack_hint *hint = p7r_stack_hint_new_blank();
    if (unlikely(hint == NULL))
        return NULL;
    return p7r_stack_hint_init_by_entrance(hint, entrance, &config);
}

// no ruin
void p7r_stack_hint_delete(struct p7r_stack_hint *hint) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    scraft_deallocate(allocator, hint);
}

struct p7r_stack_metamark *p7r_stack_allocate_with_hint(struct p7r_stack_allocator *allocator, struct p7r_stack_hint *hint) {
    struct p7r_stack_metamark *stack_mark = NULL;
    uint32_t failure = 0;
    (hint->failure_stat.measure_total < hint->failure_stat.measure_limit) && (hint->failure_stat.measure_total++);
    // TODO optimization - long zone would be used up soon, check this fact as early as possible
    switch(hint->policy) {
        case P7R_STACK_POLICY_PRUDENT:
            stack_mark = p7r_stack_page_allocate(&(allocator->long_term));
            if (stack_mark)
                return stack_mark;
            failure++;
        case P7R_STACK_POLICY_EDEN:
            stack_mark = p7r_stack_page_allocate(&(allocator->short_term));
            if (stack_mark)
                return stack_mark;
            failure++;
        default:
            stack_mark = p7r_stack_page_allocate_fallback(allocator);
    }
    hint->failure_stat.measure_failed += (failure > 0);
    return stack_mark;
}

struct p7r_stack_metamark *p7r_stack_allocate_hintless(struct p7r_stack_allocator *allocator, uint8_t policy) {
    struct p7r_stack_metamark *stack_mark = NULL;
    switch(policy) {
        case P7R_STACK_POLICY_PRUDENT:
            stack_mark = p7r_stack_page_allocate(&(allocator->long_term));
            if (stack_mark)
                return stack_mark;
        case P7R_STACK_POLICY_EDEN:
            stack_mark = p7r_stack_page_allocate(&(allocator->short_term));
            if (stack_mark)
                return stack_mark;
        default:
            stack_mark = p7r_stack_page_allocate_fallback(allocator);
    }
    return stack_mark;
}

static inline
uint64_t uint32_hashing(uint32_t key) {
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key) * 0x45d9f3b;
    key = ((key >> 16) ^ key);
    return (uint64_t) key;
}

static inline
uint64_t uint64_hashing(uint64_t key) {
    key = (key ^ (key >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    key = (key ^ (key >> 37)) * UINT64_C(0x94d049bb133111eb);
    key = key ^ (key >> 31);
    return key;
}

#if UINTPTR_MAX == 0xffffffff
#define voidptr_hashing(ptr_) uint32_hashing((uint32_t) ((uintptr_t) (ptr_)))
#elif UINTPTR_MAX == 0xffffffffffffffff
#define voidptr_hashing(ptr_) uint64_hashing((uint64_t) ((uintptr_t) (ptr_)))
#else
#error "Unsupported architecture - bad UINTPTR_MAX definition"
#endif

uint64_t p7r_stack_hint_entry_hash(struct scraft_hashkey *key) {
    struct p7r_stack_hint *hint = container_of(key, struct p7r_stack_hint, hashable);
    return (hint->key.solid_entrance) ? voidptr_hashing(hint->key.solid_entrance) : scraft_hashaux_djb_cstring(hint->key.name);
}

int p7r_stack_hint_entry_destroy(struct scraft_hashkey *key) {
    struct p7r_stack_hint *hint = container_of(key, struct p7r_stack_hint, hashable);
    p7r_stack_hint_delete(hint);
    return 0;
}

int p7r_stack_hint_entry_compare(struct scraft_hashkey *lhs_, struct scraft_hashkey *rhs_) {
    struct p7r_stack_hint *lhs = container_of(lhs_, struct p7r_stack_hint, hashable),
                          *rhs = container_of(rhs_, struct p7r_stack_hint, hashable);
    if (lhs->key.solid_entrance && rhs->key.solid_entrance) {
        // 3-way comparison
        return !(lhs->key.solid_entrance == rhs->key.solid_entrance);
    } else if (lhs->key.name && rhs->key.name) {
        return strcmp(lhs->key.name, rhs->key.name);
    }

    return -1;
}
