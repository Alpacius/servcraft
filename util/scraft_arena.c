#ifndef     SCRAFT_ARENA_TYPE
#error      "servcraft compilation failed - no type found when instantiating arena"
#endif

#include    <pthread.h>
#include    "../include/miscutils.h"
#include    "../include/util_list.h"
#include    "../include/model_alloc.h"

#ifdef      SCRAFT_ARENA_ENABLE_DEFAULT_TOKEN
static uint32_t lb_token_shared = 0;
static __thread uint32_t thread_token_cached = -1;
#define     SCRAFT_ARENA_THREAD_TOKEN   thread_token_cached
#define     try_default_token \
    do { if (unlikely(thread_token_cached < 0)) thread_token_cached = __atomic_fetch_add(&lb_token_shared, 1, __ATOMIC_ACQ_REL); } while (0)
#else
#define     try_default_token
#endif

#ifndef     SCRAFT_ARENA_THREAD_TOKEN
#error      "servcraft compilation failed - no thread token define found when instantiating arena"
#endif


static
void guard_unlock(pthread_spinlock_t **arg) {
    pthread_spin_unlock(*arg);
}

#define     scoped_guard \
    pthread_spinlock_t *guard_ptr__ __attribute__((cleanup(guard_unlock))); \
    guard_ptr__ = &(arena_instance.guards[(SCRAFT_ARENA_THREAD_TOKEN) % arena_instance.n_slots]); \
    pthread_spin_lock(guard_ptr__)


struct scraft_arena_element {
    SCRAFT_ARENA_TYPE value;
    int cached;
    list_ctl_t lctl;
};

struct scraft_arena {
    uint32_t n_slots, n_elements;
    struct scraft_model_alloc allocator;
    pthread_spinlock_t *guards;
    list_ctl_t *slots;
    struct scraft_arena_element *elements;
} arena_instance;


int scraft_arena_init(struct scraft_model_alloc allocator, uint32_t n_elements, uint32_t n_slots) {
    void local_cleanup_guards(pthread_spinlock_t **arg) { if (*arg) scraft_deallocate(allocator, (void *) *arg); }
    void local_cleanup_elements(struct scraft_arena_element **arg) { if (*arg) scraft_deallocate(allocator, *arg); }
    void local_cleanup_slots(list_ctl_t **arg) { if (*arg) scraft_deallocate(allocator, *arg); }

    (arena_instance.allocator = allocator), (arena_instance.n_elements = n_elements), (arena_instance.n_slots = n_slots);

    pthread_spinlock_t *guards __attribute__((cleanup(local_cleanup_guards)));
    struct scraft_arena_element *elements __attribute__((cleanup(local_cleanup_elements)));
    list_ctl_t *slots __attribute__((cleanup(local_cleanup_slots)));

    if (unlikely((guards = scraft_allocate(allocator, sizeof(pthread_spinlock_t) * n_slots)) == NULL))
        return -1;
    if (unlikely((elements = scraft_allocate(allocator, sizeof(struct scraft_arena_element) * n_elements)) == NULL))
        return -1;
    if (unlikely((slots = scraft_allocate(allocator, sizeof(list_ctl_t) * n_slots)) == NULL))
        return -1;

    (arena_instance.elements = elements), (arena_instance.guards = guards), (arena_instance.slots = slots);

    for (uint32_t slot_index = 0; slot_index < n_slots; slot_index++) {
        init_list_head(&(arena_instance.slots[slot_index]));
        pthread_spin_init(&(arena_instance.guards[slot_index]), PTHREAD_PROCESS_PRIVATE);
    }
    for (uint32_t element_index = 0; element_index < n_elements; element_index++) {
        uint32_t target_slot = element_index % n_slots;
        arena_instance.elements[element_index].cached = 1;
        list_add_tail(&(arena_instance.elements[element_index].lctl), &(arena_instance.slots[target_slot]));
    }
    return 0;
}

SCRAFT_ARENA_TYPE *scraft_arena_get(void) {
    try_default_token;
    uint32_t target_slot = SCRAFT_ARENA_THREAD_TOKEN % arena_instance.n_slots;
    {
        scoped_guard;
        if (!list_is_empty(&(arena_instance.slots[target_slot]))) {
            list_ctl_t *target_lctl = arena_instance.slots[target_slot].next;
            list_del(target_lctl);
            struct scraft_arena_element *element = container_of(target_lctl, struct scraft_arena_element, lctl);
            return &(element->value);
        }
    }
    struct scraft_arena_element *element_tmp = scraft_allocate(arena_instance.allocator, sizeof(struct scraft_arena_element));
    element_tmp && (element_tmp->cached = 0);
    return &(element_tmp->value);
}

void scraft_arena_release(SCRAFT_ARENA_TYPE *object) {
    struct scraft_arena_element *element = container_of(object, struct scraft_arena_element, value);
    if (element->cached) {
        try_default_token;
        uint32_t target_slot = SCRAFT_ARENA_THREAD_TOKEN % arena_instance.n_slots;
        scoped_guard;
        list_add_tail(&(element->lctl), &(arena_instance.slots[target_slot]));
    } else
        scraft_deallocate(arena_instance.allocator, object);
}
