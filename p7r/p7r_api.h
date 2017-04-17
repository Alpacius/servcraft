#ifndef     P7R_API_H_
#define     P7R_API_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_uthread.h"


int p7r_poolization_status(void);
int p7r_poolize(struct p7r_config config);
int p7r_execute(void (*entrance)(void *), void *argument, void (*dtor)(void *));

#endif      // P7R_API_H_
