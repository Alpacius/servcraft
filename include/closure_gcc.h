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
    void * _abs_target_ = _base_ip_ + (uintptr_t) *((void **) &tramp__[ORIG_POS2]); \
    *((void **) (&tramp__[TRAMP_POS2])) = (void *) (tramp__ + TRAMP_CODESIZE); \
    *((void **) (&tramp__[TRAMP_CODESIZE])) = _abs_target_; \
    tramp__[TRAMP_POS2 - 2] = 0xff; \
    tramp__[TRAMP_POS2 - 1] = 0x25; \
} while(0)
#endif

// XXX Now we have a stable frame pointer. Thanks to lh_mouse.

#define _closure(src_)    ({ void *(*alloc__)(size_t) = (src_); 
#define _lambda(rt_, ...) \
        rt_ $(__VA_ARGS__)
#define _closure_end    \
        void *sp__, *bp__, *cntx__; \
        *(volatile char *)__builtin_alloca(sizeof(char)) = 0; \
        asm volatile (GETSP : "=r" (sp__)); \
        asm volatile (GETBP : "=r" (bp__)); \
        char *tramp__ = alloc__(sizeof(char) * (TRAMP_SIZE + (unsigned long) (bp__-sp__))); \
        memcpy(tramp__, $, TRAMP_SIZE); \
        cntx__ = *((void **) (&tramp__[TRAMP_POS])); \
        memcpy(tramp__ + TRAMP_SIZE, cntx__, (unsigned long) (bp__ - cntx__)); \
        *((void **) (&tramp__[TRAMP_POS])) = \
                (void *) (tramp__ + TRAMP_SIZE); \
        jmp_encode; \
        (typeof(&$)) ((void *) tramp__); })

#endif      // CLOSURE_GCC_H_
