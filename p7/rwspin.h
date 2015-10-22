#ifndef     RWSPIN_H_
#define     RWSPIN_H_

#include    "./p7impl.h"

struct p7_rwspinlock {
    uint32_t request, n_readers;
    volatile uint32_t completion;
    uint32_t spintime;
};

void p7_rwspinlock_init(struct p7_rwspinlock *rwspin, uint32_t spintime);
void p7_rwspinlock_rdlock(struct p7_rwspinlock *rwspin);
void p7_rwspinlock_rdunlock(struct p7_rwspinlock *rwspin);
void p7_rwspinlock_wrlock(struct p7_rwspinlock *rwspin);
void p7_rwspinlock_wrunlock(struct p7_rwspinlock *rwspin);

#endif      // RWSPIN_H_
