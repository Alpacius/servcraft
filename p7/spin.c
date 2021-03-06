#include    "./p7intern.h"
#include    "./spin.h"
#include    "./p7_root_alloc.h"
#include    "../include/miscutils.h"
#include    "../include/model_alloc.h"


static struct p7_spinlock *reserved = NULL;
static uint32_t n_main_reserved = 0;
static __thread
struct {
    uint32_t begin_offset, cap, used, next_avail;
    uint64_t cap_cached, free_cached;
    list_ctl_t cached;
} arena_info;

int p7_spinlock_preinit(uint32_t n_reserved) {
    __auto_type allocator = p7_root_alloc_get_proxy();
    int ret = -1;
    reserved = scraft_allocate(allocator, sizeof(struct p7_spinlock) * n_reserved);
    if (reserved != NULL) {
        (ret = 0), (n_main_reserved = n_reserved);
        for (int32_t idx = 0; idx < n_reserved; idx++)
            (reserved[idx].idx_reserved = idx), (reserved[idx].is_free = P7_SPINLOCK_FREE);
    }
    return ret;
}

void p7_spinlock_tlinit(void *cap_cached) {
    (arena_info.cap = n_main_reserved / p7_get_ncarriers()), (arena_info.begin_offset = arena_info.cap * p7_carrier_self_tl()->carrier_id), (arena_info.used = arena_info.next_avail = 0);
    (arena_info.cap_cached = (uint64_t) cap_cached), (arena_info.free_cached = 0);
    arena_info.next_avail = arena_info.begin_offset;
    init_list_head(&(arena_info.cached));
}

static
struct p7_spinlock *p7_spinlock_thunk_alloc(void) {
    struct p7_spinlock *spin = NULL;
    if (arena_info.used < arena_info.cap) {
        arena_info.used++;
        uint32_t idx = arena_info.next_avail;
        do {
            if (reserved[idx].is_free == P7_SPINLOCK_FREE) {
                spin = &(reserved[idx]);
                (arena_info.used++), (arena_info.next_avail = idx);
                spin->is_free = P7_SPINLOCK_BUSY;
                break;
            }
            idx = (idx + 1) % arena_info.cap;
        } while (idx != arena_info.next_avail);
    } else if (arena_info.free_cached) {
        spin = container_of(arena_info.cached.next, struct p7_spinlock, lctl);
        list_del(arena_info.cached.next);
        arena_info.free_cached--;
        spin->lock = 0;
    } else {
        __auto_type allocator = p7_root_alloc_get_proxy();
        ((spin = scraft_allocate(allocator, sizeof(struct p7_spinlock))) != NULL) && (spin->is_free = P7_SPINLOCK_TEMP);
    }
    return spin;
}

struct p7_spinlock *p7_spinlock_create(uint32_t spintime) {
    struct p7_spinlock *spin = p7_spinlock_thunk_alloc();
    (spin != NULL) && ((__atomic_store_n(&(spin->lock), 0, __ATOMIC_SEQ_CST), 0), (spin->spintime = spintime), (spin->from = p7_carrier_self_tl()->carrier_id));
    return spin;
}

// since 0.2.0
void p7_spinlock_init(struct p7_spinlock *spin, uint32_t spintime) {
    __atomic_store_n(&(spin->lock), 0, __ATOMIC_RELEASE);
    (spin->spintime = spintime), (spin->from = p7_carrier_self_tl()->carrier_id), (spin->is_free = P7_SPINLOCK_LOCAL);
}

void p7_spinlock_destroy(struct p7_spinlock *spin) {
    if (likely(spin->from == p7_carrier_self_tl()->carrier_id)) {
        switch(spin->is_free) {
            case P7_SPINLOCK_BUSY:
            {
                (spin->lock = 0), (spin->is_free = P7_SPINLOCK_FREE);
                arena_info.next_avail = spin->idx_reserved;
                break;
            }
            case P7_SPINLOCK_TEMP:
            {
                if (arena_info.free_cached < arena_info.cap_cached) {
                    arena_info.free_cached++;
                    list_add_tail(&(spin->lctl), &(arena_info.cached));
                    spin->lock = 0;
                } else {
                    __auto_type allocator = p7_root_alloc_get_proxy();
                    scraft_deallocate(allocator, spin);
                }
                break;
            }
            case P7_SPINLOCK_LOCAL:     // XXX reserved
            default:
                break;
        }
    }
}

__asm__(".symver p7_spinlock_lock_0_1,p7_spinlock_lock@LIBP7_0.1");
void p7_spinlock_lock_0_1(struct p7_spinlock *spin) {
    uint32_t ret, spincount;
    for (;;) {
        spincount = 0;
        do {
            (ret = __atomic_exchange_n(&(spin->lock), 1, __ATOMIC_SEQ_CST)) && (spincount++);
        } while (ret && (spin->spintime - spincount));
        if (ret == 0)
            break;
        p7_coro_yield();
    }
}

__asm__(".symver p7_spinlock_lock_0_2,p7_spinlock_lock@@LIBP7_0.2");
void p7_spinlock_lock_0_2(struct p7_spinlock *spin) {
    uint32_t spincount, penalty = 0;
    //while(__atomic_exchange_n(&(spin->lock), 1, __ATOMIC_SEQ_CST)) {
    while (__atomic_load_n(&(spin->lock), __ATOMIC_CONSUME) ||
           __atomic_exchange_n(&(spin->lock), 1, __ATOMIC_ACQ_REL)) {
        spincount = 0;
#ifdef  P7_USE_INTEL_PAUSE
#define cpu_relax   __builtin_ia32_pause()
#else
#define cpu_relax
#endif
        (penalty && (p7_coro_yield(), penalty)), (penalty = 1 - penalty);
        while (spin->spintime - spincount++)
            cpu_relax;
    }
#undef cpu_relax
}

void p7_spinlock_unlock(struct p7_spinlock *spin) {
    __atomic_store_n(&(spin->lock), 0, __ATOMIC_RELEASE);
    p7_coro_yield();
}
