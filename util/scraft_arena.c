#include    "./scraft_arena.h"
#include    "./scraft_poolize.h"


static __thread struct scraft_object_pool arena_tl_pool;


static inline
struct scraft_arena_large *large_block_new(struct scraft_model_alloc allocator, uint32_t size) {
    return scraft_allocate(allocator, sizeof(struct scraft_arena_large) + sizeof(char) * size);
}

static inline
void large_block_delete(struct scraft_model_alloc allocator, struct scraft_arena_large *block) {
    scraft_deallocate(allocator, block);
}

static
struct scraft_arena_elt *arena_block_new(struct scraft_model_alloc allocator, uint32_t size, uint32_t nfail) {
    struct scraft_arena_elt *arena_block = scraft_allocate(allocator, sizeof(struct scraft_arena_elt) + sizeof(char) * size);
    if (arena_block != NULL) {
        (arena_block->size = size), (arena_block->nfail = nfail);
        (arena_block->pos = 0), (arena_block->failed = 0);
    }
    return arena_block;
}

static
void arena_block_delete(struct scraft_model_alloc allocator, struct scraft_arena_elt *arena_block) {
    scraft_deallocate(allocator, arena_block);
}

static
struct scraft_arena_elt *arena_block_get(struct scraft_model_alloc allocator, uint32_t size, uint32_t nfail) {
    list_ctl_t *elt = scraft_pool_get(&arena_tl_pool);
    if (elt != NULL) {
        struct scraft_arena_elt *arena_block = container_of(elt, struct scraft_arena_elt, lctl);
        (arena_block->pos = arena_block->failed = 0), (arena_block->nfail = nfail);         // XXX We do not reset size. However 'tis wrong still.
        return arena_block;
    }
    return arena_block_new(allocator, size, nfail);
}

static
void arena_block_put(struct scraft_model_alloc allocator, struct scraft_arena_elt *arena_block) {
    // XXX since arena_block_get resets pos, failed and nfail, we care nothing here.
    if (scraft_pool_put(&arena_tl_pool, &(arena_block->lctl)) == POOL_PUT_FAIL_FULL)
        arena_block_delete(allocator, arena_block);
}

struct scraft_arena *scraft_arena_new(struct scraft_model_alloc allocator, uint32_t basesize, uint32_t nfail, uint32_t nlarge, uint32_t nbasehooks) {
    struct scraft_arena *arena = scraft_allocate(allocator, sizeof(struct scraft_arena));
    if (arena != NULL) {
        scraft_deallocate(allocator, ({ arena->dtor_chain = scraft_allocate(allocator, sizeof(struct scraft_arena_dtor_chain) + sizeof(struct scraft_arena_dtor_hook) * nbasehooks); if (likely(arena->dtor_chain != NULL)) goto hook_success; arena; }));
        return NULL;
hook_success:
        (arena->basesize = basesize), (arena->nfail = nfail), (arena->nlarge = nlarge), (arena->dtor_chain->nbase = nbasehooks), (arena->dtor_chain->ncurrent = 0), (arena->allocator = allocator);
        init_list_head(&(arena->arena_set));
        init_list_head(&(arena->large_block_set));
        init_list_head(&(arena->deprecated_set));
        init_list_head(&(arena->dtor_chain->extra_hooks));
        struct scraft_arena_elt *arena_block = arena_block_get(allocator, basesize, nfail);
        if (likely(arena_block != NULL))
            list_add_head(&(arena_block->lctl), &(arena->arena_set));
    }
    return arena;
}

void scraft_arena_delete(struct scraft_arena *arena) {
    list_ctl_t *p, *t;
    // XXX All destruction invocations are performed before the actual deallocation. Beware of race.
    uint32_t idx;
    for (idx = 0; idx < arena->dtor_chain->ncurrent; idx++)
        arena->dtor_chain->base_hooks[idx].dtor(arena->dtor_chain->base_hooks[idx].this_ptr, arena->dtor_chain->base_hooks[idx].user_arg);
    if (idx && (idx == arena->dtor_chain->nbase)) {
        list_foreach_remove(p, &(arena->dtor_chain->extra_hooks), t) {
            list_del(t);
            struct scraft_arena_dtor_hook *hook = container_of(t, struct scraft_arena_dtor_hook, lctl);
            hook->dtor(hook->this_ptr, hook->user_arg);
            scraft_deallocate(arena->allocator, hook);
        }
    }
    list_foreach_remove(p, &(arena->large_block_set), t) {
        list_del(t);
        large_block_delete(arena->allocator, container_of(t, struct scraft_arena_large, lctl));
    }
    list_foreach_remove(p, &(arena->arena_set), t) {
        list_del(t);
        arena_block_put(arena->allocator, container_of(t, struct scraft_arena_elt, lctl));
    }
    list_foreach_remove(p, &(arena->deprecated_set), t) {
        list_del(t);
        arena_block_put(arena->allocator, container_of(t, struct scraft_arena_elt, lctl));
    }
    scraft_deallocate(arena->allocator, arena->dtor_chain);
    scraft_deallocate(arena->allocator, arena);
}

void *scraft_arena_allocate(struct scraft_arena *arena, size_t blocksize) {
    // XXX Not aligned.
    char *block = NULL;
    if (unlikely(list_is_empty(&(arena->arena_set)))) {
        struct scraft_arena_elt *arena_block = arena_block_get(arena->allocator, arena->basesize, arena->nfail);
        if (unlikely(arena_block == NULL))
            return NULL;
        list_add_tail(&(arena_block->lctl), &(arena->arena_set));
    }
    if (blocksize < arena->nlarge) {
        struct scraft_arena_elt *target_block = container_of(arena->arena_set.next, struct scraft_arena_elt, lctl);
        if (target_block->size - target_block->pos >= blocksize) {
            block = &(target_block->pool[target_block->pos]);
            target_block->pos += blocksize;
        } else {
            list_del(&(target_block->lctl));
            block = scraft_arena_allocate(arena, blocksize);
            if (++target_block->failed <= target_block->nfail)
                list_add_head(&(target_block->lctl), &(arena->arena_set));
            else
                list_add_head(&(target_block->lctl), &(arena->deprecated_set));
        }
    } else {
        struct scraft_arena_large *large_block = large_block_new(arena->allocator, blocksize);
        block = (likely(large_block != NULL)) ? ({ list_add_tail(&(large_block->lctl), &(arena->large_block_set)); large_block->block; }) : NULL;
    }
    return block;
}

void scraft_arena_tlinit(uint32_t cap) {
    (arena_tl_pool.size = 0), (arena_tl_pool.cap = cap);
    init_list_head(&(arena_tl_pool.pool));
}

struct scraft_arena_response scraft_arena_delay_dtor(struct scraft_arena *arena, void *object, void (*dtor)(void *, void *), void *user_arg) {
    struct scraft_arena_dtor_chain *chain = arena->dtor_chain;
    struct scraft_arena_dtor_hook *target_hook = NULL;
    struct scraft_arena_response ret = { .object = object, .success = 1 };
    if (chain->ncurrent < chain->nbase) {
        target_hook = &(chain->base_hooks[chain->ncurrent++]);
    } else {
        struct scraft_arena_dtor_hook *hook = scraft_allocate(arena->allocator, sizeof(struct scraft_arena_dtor_hook));
        if (unlikely(hook == NULL)) {
            ret.success = 0;
            return ret;
        }
        list_add_tail(&(hook->lctl), &(chain->extra_hooks));
        target_hook = hook;
    }
    (target_hook->this_ptr = object), (target_hook->user_arg = user_arg), (target_hook->dtor = dtor);
    return ret;
}
