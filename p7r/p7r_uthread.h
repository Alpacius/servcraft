#ifndef     P7R_UTHREAD_H_
#define     P7R_UTHREAD_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"
#include    "./p7r_uthread_def.h"


int p7r_init(struct p7r_config config);
struct p7r_delegation p7r_delegate(uint64_t events, ...);
void p7r_yield(void);
int p7r_uthread_create(void (*entrance)(void *), void *argument, void (*dtor)(void *), int yield);

int p7r_uthread_create_foreign(uint32_t target_carrier_index, void (*entrance)(void *), void *argument, void (*dtor)(void *), struct p7r_future *future);

struct p7r_carrier *p7r_carriers();
uint32_t balanced_target_carrier(void);
uint32_t p7r_n_carriers(void);

struct p7r_future *p7r_get_future(void);

#endif      // P7R_UTHREAD_H_
