#ifndef     P7R_FUTURE_H_
#define     P7R_FUTURE_H_

#include    "./p7r_future_def.h"


static inline
int p7r_future_init(struct p7r_future *future) {
    return (future->error_code = 0), (future->result = NULL), sem_init(&(future->sync_barrier), 0, 0);
}

static inline
int p7r_future_ruin(struct p7r_future *future) {
    return sem_destroy(&(future->sync_barrier));
}

static inline
void *p7r_future_wait(struct p7r_future *future) {
    if (sem_wait(&(future->sync_barrier)) == -1) {
        return NULL;
    }
    return __atomic_load_n(&(future->result), __ATOMIC_SEQ_CST);;
}

static inline
void *p7r_future_trywait(struct p7r_future *future) {
    if (sem_trywait(&(future->sync_barrier)) == -1) {
        return NULL;
    }
    return __atomic_load_n(&(future->result), __ATOMIC_SEQ_CST);;
}

static inline
void *p7r_future_timedwait(struct p7r_future *future, struct timespec abs_timeout) {
    if (sem_timedwait(&(future->sync_barrier), &abs_timeout) == -1) {
        return NULL;
    }
    return __atomic_load_n(&(future->result), __ATOMIC_SEQ_CST);;
}

static inline
void *p7r_future_load_result(struct p7r_future *future) {
    return __atomic_load_n(&(future->result), __ATOMIC_SEQ_CST);;
}

static inline
void p7r_future_store_result(struct p7r_future *future, void *result) {
    __atomic_store_n(&(future->result), result, __ATOMIC_SEQ_CST);
}

static inline
int p7r_future_post(struct p7r_future *future) {
    return sem_post(&(future->sync_barrier));
}

static inline
int p7r_future_get_error(struct p7r_future *future) {
    return __atomic_load_n(&(future->error_code), __ATOMIC_SEQ_CST);
}

static inline
void p7r_future_set_error(struct p7r_future *future, int error_code) {
    return __atomic_store_n(&(future->error_code), error_code, __ATOMIC_SEQ_CST);
}


#endif      // P7R_FUTURE_H_
