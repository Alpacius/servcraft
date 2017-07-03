#ifndef     P7R_FUTURE_DEF_H_
#define     P7R_FUTURE_DEF_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

#include    <semaphore.h>


struct p7r_future {
    void *result;
    int error_code;
    sem_t sync_barrier;
    list_ctl_t lctl;
};

#endif      // P7R_FUTURE_DEF_H_
