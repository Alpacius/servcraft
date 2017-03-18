#ifndef     P7R_STACK_ALLOCATOR_
#define     P7R_STACK_ALLOCATOR_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"

#define     P7R_STACK_ALLOCATOR_MASTER      0
#define     P7R_STACK_ALLOCATOR_SLAVE       1

#define     P7R_STACK_SOURCE_DEFAULT        0
#define     P7R_STACK_SOURCE_SHORT_TERM     1

#include    "./p7r_stack_metamark.h"


struct p7r_stack_allocator_config {
    uint32_t n_pages_long_term, n_pages_short_term;
    uint32_t n_pages_slave;
    uint32_t n_pages_stack_total, n_pages_stack_user;
    uint32_t n_bytes_page;
};

struct p7r_stack_page_provider {
    uint32_t type;
    uint32_t size, capacity, n_bytes_page;
    size_t total_size;
    char *zone;
    struct p7r_stack_allocator *parent;
    list_ctl_t linkable, pages;
};

struct p7r_stack_page_slaver {
    uint32_t n_slaves;
    list_ctl_t slaves;
};

struct p7r_stack_allocator {
    struct p7r_stack_allocator_config properties;
    struct p7r_stack_page_provider long_term, short_term;
    struct p7r_stack_page_slaver slaves;
};

struct p7r_stack_allocator *p7r_stack_allocator_init(struct p7r_stack_allocator *allocator, struct p7r_stack_allocator_config config);
void p7r_stack_allocator_ruin(struct p7r_stack_allocator *allocator);

struct p7r_stack_metamark *p7r_stack_allocate(int type, struct p7r_stack_allocator *allocator);
void p7r_stack_free(struct p7r_stack_metamark *mark);

struct p7r_stack_metamark *p7r_stack_page_allocate_fallback(struct p7r_stack_allocator *allocator);
struct p7r_stack_metamark *p7r_stack_page_allocate(struct p7r_stack_page_provider *provider);
void p7r_stack_page_free(struct p7r_stack_metamark *mark);

double p7r_stack_allocator_usage(struct p7r_stack_allocator *allocator);



#endif      // P7R_STACK_ALLOCATOR_
