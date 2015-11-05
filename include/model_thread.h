#ifndef     MODEL_THREAD_H_
#define     MODEL_THREAD_H_

#include    <stddef.h>
#include    <stdint.h>
#include    "miscutils.h"

// NOTE: writer-first

struct scraft_model_rwlock {
    union {
        void (*plain_ptr_)(void *);
        void (*closure_)(void);
    } rdlock_, wrlock_, unlock_;
    void *lock_;
};

struct scraft_model_mutex {
    union {
        void (*plain_ptr_)(void *);
        void (*closure_)(void);
    } lock_, unlock_;
    void *mutex_;
};

#define scraft_lock_invoke_(model_, member_, ...) \
    _if__(__VA_NARG__(__VA_ARGS__))( ((model_).member_.plain_ptr_(__VA_ARGS__)), ((model_).member_.closure_(__VA_ARGS__)) )

#define scraft_rwlock_rdlock(model_, ...) scraft_lock_invoke_(model_, rdlock_, __VA_ARGS__)
#define scraft_rwlock_wrlock(model_, ...) scraft_lock_invoke_(model_, wrlock_, __VA_ARGS__)
#define scraft_rwlock_unlock(model_, ...) scraft_lock_invoke_(model_, unlock_, __VA_ARGS__)

#define scraft_mutex_lock(model_, ...) scraft_lock_invoke_(model_, lock_, __VA_ARGS__)
#define scraft_mutex_unlock(model_, ...) scraft_lock_invoke_(model_, unlock_, __VA_ARGS__)

#endif      // MODEL_THREAD_H_
