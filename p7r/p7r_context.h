#ifndef     P7R_CONTEXT_H_
#define     P7R_CONTEXT_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

// TODO conditional compliation
#include    "./p7r_mcontext_x64.h"


/*
 * Machine-independent & policy-free context structure, one per uthread.
 */

struct p7r_context {
    struct p7r_mcontext mcontext;
    char *stack_base;
    size_t stack_size;      // in bytes
};

static inline
struct p7r_context *p7r_context_init(struct p7r_context *context, char *stack_base, size_t stack_size) {
    (context->stack_base = stack_base), (context->stack_size = stack_size);
    return context;
}

static inline
struct p7r_context *p7r_context_prepare(struct p7r_context *context, void (*entrance)(void *), void *argument) {
    p7r_mcontext_init(&(context->mcontext), entrance, argument, p7r_mcontext_stack_base(context->stack_base, context->stack_size));
    return context;
}

#define p7r_context_switch(to_, from_)  p7r_mcontext_switch(&((to_)->mcontext), &((from_)->mcontext))

#endif      // P7R_CONTEXT_H_
