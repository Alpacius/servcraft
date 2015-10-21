#ifndef     P7_SPIN_H_
#define     P7_SPIN_H_

#include    "./p7impl.h"

struct p7_spinlock {
    uint32_t lock, spintime;
    int64_t idx_reserved;
    int is_free;
    uint32_t from;
    list_ctl_t lctl;
};

#define     P7_SPINLOCK_BUSY        0
#define     P7_SPINLOCK_FREE        1
#define     P7_SPINLOCK_TEMP        2
#define     P7_SPINLOCK_LOCAL       3

int p7_spinlock_preinit(uint32_t n_reserved);
void p7_spinlock_tlinit(void *cap_cached);
void p7_spinlock_init(struct p7_spinlock *spin, uint32_t spintime);
struct p7_spinlock *p7_spinlock_create(uint32_t spintime);
void p7_spinlock_destroy(struct p7_spinlock *spin);
void p7_spinlock_lock(struct p7_spinlock *spin);
void p7_spinlock_unlock(struct p7_spinlock *spin);

#endif      // P7_SPIN_H_
