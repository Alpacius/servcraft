#include    "./rwspin.h"
#include    "./p7intern.h"

#include    <stdio.h>

void p7_rwspinlock_init(struct p7_rwspinlock *rwspin, uint32_t spintime) {
    __atomic_store_n(&(rwspin->request), 0, __ATOMIC_RELEASE);
    __atomic_store_n(&(rwspin->n_readers), 0, __ATOMIC_RELEASE);
    (rwspin->completion = 0), (rwspin->spintime = spintime);
}

void p7_rwspinlock_rdlock(struct p7_rwspinlock *rwspin) {
    uint32_t req_id = __atomic_fetch_add(&(rwspin->request), 1, __ATOMIC_RELEASE), spincount;
    volatile int reqflg;
    do {
        spincount = 0;
        while ((reqflg = (__atomic_load_n(&(rwspin->completion), __ATOMIC_ACQUIRE) != req_id)) && (rwspin->spintime - spincount++));
        if (reqflg) p7_coro_yield();
    } while (reqflg);
    __atomic_add_fetch(&(rwspin->n_readers), 1, __ATOMIC_RELEASE);
    rwspin->completion++;
}

void p7_rwspinlock_rdunlock(struct p7_rwspinlock *rwspin) {
    __atomic_sub_fetch(&(rwspin->n_readers), 1, __ATOMIC_RELEASE);
    p7_coro_yield();
}

void p7_rwspinlock_wrlock(struct p7_rwspinlock *rwspin) {
    uint32_t req_id = __atomic_fetch_add(&(rwspin->request), 1, __ATOMIC_RELEASE), spincount;
    volatile int reqflg;
    do {
        spincount = 0;
        while ((reqflg = (__atomic_load_n(&(rwspin->completion), __ATOMIC_ACQUIRE) != req_id)) && (rwspin->spintime - spincount++));
        if (reqflg) p7_coro_yield();
    } while (reqflg);
    do {
        spincount = 0;
        while ((reqflg = (__atomic_load_n(&(rwspin->n_readers), __ATOMIC_ACQUIRE) > 0)) && (rwspin->spintime - spincount++));
        if (reqflg) p7_coro_yield();
    } while (reqflg);
}

void p7_rwspinlock_wrunlock(struct p7_rwspinlock *rwspin) {
    rwspin->completion++;
    p7_coro_yield();
}
