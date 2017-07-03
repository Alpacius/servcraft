#ifndef     P7R_API_H_
#define     P7R_API_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_uthread.h"

#include    "./p7r_future.h"


int p7r_poolization_status(void);
int p7r_poolize(struct p7r_config config);
int p7r_execute(void (*entrance)(void *), void *argument, void (*dtor)(void *));

struct p7r_future *p7r_submit(void (*entrance)(void *), void *argument, void (*dtor)(void *));
void p7r_future_release(struct p7r_future *future);

static inline
void p7r_future_release_scoped(struct p7r_future **arg) {
    p7r_future_release(*arg);
}

#define     async_local     __attribute__((cleanup(p7r_future_release_scoped)))

#endif      // P7R_API_H_
