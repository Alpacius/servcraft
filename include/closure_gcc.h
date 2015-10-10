#ifndef     CLOSURE_GCC_H_
#define     CLOSURE_GCC_H_

#include    <stdint.h>
#include    <string.h>

#ifdef      __x86_64__
#define     TRAMP_SIZE      19
#define     TRAMP_POS       8
#define     GETSP           "movq %%rsp, %0"
#define     GETBP           "movq %%rbp, %0"
#define     jmp_encode      ;
#else
#define     TRAMP_SIZE      15
#define     TRAMP_CODESIZE  11
#define     TRAMP_POS       1
#define     TRAMP_POS2      7
#define     ORIG_POS2       6
#define     GETSP           "movl %%esp, %0"
#define     GETBP           "movl %%ebp, %0"
#define     jmp_encode      \
do { \
    void * _base_ip_ = $ + 10; \
    void * _abs_target_ = _base_ip_ + (uintptr_t) *((void **) &_tramp_[ORIG_POS2]); \
    *((void **) (&_tramp_[TRAMP_POS2])) = (void *) (_tramp_ + TRAMP_CODESIZE); \
    *((void **) (&_tramp_[TRAMP_CODESIZE])) = _abs_target_; \
    _tramp_[TRAMP_POS2 - 2] = 0xff; \
    _tramp_[TRAMP_POS2 - 1] = 0x25; \
} while(0)
#endif

// XXX Now we have a stable frame pointer. Thanks to lh_mouse.

#define _closure(_src_)    ({ void *(*_alloc_)(size_t) = (_src_); 
#define _lambda(_rt_, ...) \
        _rt_ $(__VA_ARGS__)
#define _closure_end    \
        void *_sp_, *_bp_, *_cntx_; \
        *(volatile char *)__builtin_alloca(sizeof(char)) = 0; \
        asm volatile (GETSP : "=r" (_sp_)); \
        asm volatile (GETBP : "=r" (_bp_)); \
        char *_tramp_ = _alloc_(sizeof(char) * (TRAMP_SIZE + (unsigned long) (_bp_-_sp_))); \
        memcpy(_tramp_, $, TRAMP_SIZE); \
        _cntx_ = *((void **) (&_tramp_[TRAMP_POS])); \
        memcpy(_tramp_ + TRAMP_SIZE, _cntx_, (unsigned long) (_bp_ - _cntx_)); \
        *((void **) (&_tramp_[TRAMP_POS])) = \
                (void *) (_tramp_ + TRAMP_SIZE); \
        jmp_encode; \
        (typeof(&$)) ((void *) _tramp_); })

#endif      // CLOSURE_GCC_H_
