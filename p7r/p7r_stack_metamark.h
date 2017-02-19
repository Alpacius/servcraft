#ifndef     P7R_STACK_METAMARK_H_
#define     P7R_STACK_METAMARK_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

struct p7r_stack_allocator;

struct p7r_stack_metamark {
    struct p7r_stack_page_provider *provider;
    uint32_t n_bytes_page;
    char *raw_content_addr, *red_zone_addr;
    list_ctl_t linkable;
    char user_metadata[];
} __attribute__((packed));


#endif      // P7R_STACK_METAMARK_H_
