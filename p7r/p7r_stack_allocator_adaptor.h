#ifndef     P7R_STACK_ALLOCATOR_ADAPTOR_H_
#define     P7R_STACK_ALLOCATOR_ADAPTOR_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

#include    "./p7r_stack_metamark.h"

#ifndef     P7R_USE_SPLIT_STACK

#include    "./p7r_stack_allocator.h"
#include    "./p7r_stack_hint.h"

#define     stack_metamark_create       p7r_stack_allocate_hintless
#define     stack_metamark_destroy      p7r_stack_free

#define     stack_allocator_init        p7r_stack_allocator_init
#define     stack_allocator_ruin        p7r_stack_allocator_ruin

#define     stack_size_of(metamark_)    \
    ({ \
        __auto_type metamark__ = (metamark_); \
        metamark__->n_bytes_page * (metamark__->provider->parent->properties.n_pages_stack_user); \
    })
#define     stack_base_of(metamark_)    ((metamark_)->raw_content_addr)
#define     stack_meta_of(metamark_)    ((metamark_)->user_metadata)

#define     stack_usage_of              p7r_stack_allocator_usage

#else

#error      "Split stack is not supported yet."

#endif

#endif      // P7R_STACK_ALLOCATOR_ADAPTOR_H_
