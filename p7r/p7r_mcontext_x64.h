#ifndef     P7R_MCONTEXT_X64_H_
#define     P7R_MCONTEXT_X64_H_

#ifndef     __x86_64__
#error      "Wrong architecture: x86-64 expected."
#endif

#ifdef      p7r_mcontext
#error      "p7r_mcontext already defined."
#endif

#define     p7r_mcontext                p7r_mcontext_x64

#define     p7r_mcontext_init           p7r_mcontext_x64_init
#define     p7r_mcontext_switch         p7r_mcontext_x64_switch
#define     p7r_mcontext_stack_base     p7r_mcontext_x64_stack_base

#include    "./p7r_stdc_common.h"

/*
 * Userland CPU context for x86-64/linux.
 *
 * There's no going back - the only way in & out is the function initially called and its way out is to crash.
 * It is up to the wrapper to decide how to escape since no context could easily self-destruct, but
 * know that
 * no one lives forever.
 */

struct p7r_mcontext_x64 {
    uint64_t rdi, rsi, rdx, rcx, r8, r9, rax, rbx, r10;
    uint64_t rsp, rbp;
    uint64_t r11, r12, r13, r14, r15;
    uint64_t rip;
} __attribute__((packed));

void p7r_mcontext_x64_init(struct p7r_mcontext_x64 *mcontext, void (*entrance)(void *), void *argument, void *stack_base_real);
void p7r_mcontext_x64_switch(struct p7r_mcontext_x64 *to, struct p7r_mcontext_x64 *from);

static inline
void *p7r_mcontext_x64_stack_base(void *stack_base, size_t stack_size) {
    return stack_base + stack_size - 2 * sizeof(void *);
}

#endif      // P7R_MCONTEXT_X64_H_
