#ifndef     __x86_64__
#error      "Wrong architecture: x86-64 expected."
#endif

// void p7r_mcontext_x64_init(struct p7r_mcontext_x64 *mcontext, void (*entrance)(void *), void *argument, void *stack_base_real);
.globl p7r_mcontext_x64_init
p7r_mcontext_x64_init:
    movq    %rsi, 128(%rdi)
    movq    %rcx, 72(%rdi)
    movq    %rdx, (%rdi)
    movq    %rsi, (%rcx)
    ret

// void p7r_mcontext_x64_switch(struct p7r_mcontext_x64 *to, struct p7r_mcontext_x64 *from);
.globl p7r_mcontext_x64_switch
p7r_mcontext_x64_switch:
    movq    %rdi, (%rsi)
    movq    %rsi, 8(%rsi)
    movq    %rdx, 16(%rsi)
    movq    %rcx, 24(%rsi)
    movq    %r8, 32(%rsi)
    movq    %r9, 40(%rsi)
    movq    %rax, 48(%rsi)
    movq    %rbx, 56(%rsi)
    movq    %r10, 64(%rsi)
    movq    %rbp, 80(%rsi)
    movq    %r11, 88(%rsi)
    movq    %r12, 96(%rsi)
    movq    %r13, 104(%rsi)
    movq    %r14, 112(%rsi)
    movq    %r15, 120(%rsi)
    movq    %rsp, 72(%rsi)

    movq    8(%rdi), %rsi
    movq    16(%rdi), %rdx
    movq    24(%rdi), %rcx
    movq    32(%rdi), %r8
    movq    40(%rdi), %r9
    movq    48(%rdi), %rax
    movq    56(%rdi), %rbx
    movq    64(%rdi), %r10
    movq    72(%rdi), %rsp
    movq    80(%rdi), %rbp
    movq    88(%rdi), %r11
    movq    96(%rdi), %r12
    movq    104(%rdi), %r13
    movq    112(%rdi), %r14
    movq    120(%rdi), %r15
    movq    (%rdi), %rdi

    ret
