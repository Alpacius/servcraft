#ifndef     MODEL_ALLOC_H_
#define     MODEL_ALLOC_H_

#include    <stddef.h>
#include    <stdint.h>
#include    "miscutils.h"

struct scraft_model_alloc {
    union {
        void *(*plain_ptr_)(void *, size_t);
        void *(*closure_)(size_t);      // XXX or malloc
    } allocator_;
    union {
        void (*plain_ptr_)(void *, void *);
        void (*closure_)(void *);      // XXX or free
    } deallocator_;
    union {
        void *(*plain_ptr_)(void *, void *, size_t);
        void *(*closure_)(void *, size_t);   // XXX or realloc
    } reallocator_;
    void *udata_;
};

#define scraft_allocate(model_, arg_, ...) \
    _Generic((arg_), \
             void *: (model_).allocator_.plain_ptr_, \
             size_t: (model_).allocator_.closure_)((arg_), ##__VA_ARGS__)

#define scraft_deallocate(model_, ...) \
    _if(__VA_NARG__(__VA_ARGS__))( ((model_).deallocator_.plain_ptr_(__VA_ARGS__)), ((model_).deallocator_.closure_(__VA_ARGS__)) )

#define scraft_reallocate(model_, ...) \
    _if_(__VA_NARG__(__VA_ARGS__))( ((model_).reallocator_.plain_ptr_(__VA_ARGS__)), ((model_).reallocator_.closure_(__VA_ARGS__)) )

#endif      // MODEL_ALLOC_H_
