#include    "./p7r_stack_allocator.h"
#include    "./p7r_root_alloc.h"


static
struct p7r_stack_page_provider *p7r_stack_page_provider_init(
        struct p7r_stack_page_provider *provider, 
        uint32_t type,
        uint32_t n_pages_capacity, 
        uint32_t n_pages_stack,
        uint32_t n_bytes_page,
        struct p7r_stack_allocator *parent
    ) {
    // provider->total_size is just some read-only hint
    if (unlikely(__builtin_mul_overflow(n_pages_capacity, n_bytes_page, &(provider->total_size)))) {
        provider->zone = NULL;
        return NULL;
    }

    provider->zone = mmap(NULL, provider->total_size, PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE|MAP_ANONYMOUS, -1, 0);
    if (provider->zone == MAP_FAILED) {
        provider->zone = NULL;
        return NULL;
    }

    provider->size = provider->capacity = n_pages_capacity;
    provider->n_bytes_page = n_bytes_page;
    provider->type = type;
    provider->parent = parent;
    init_list_head(&(provider->pages));

    void *stack_page_iterator = provider->zone;
    for (
         uint32_t n_pages_initialized = 0; 
         n_pages_initialized < n_pages_capacity;
         n_pages_initialized += n_pages_stack, stack_page_iterator += (n_pages_stack * n_bytes_page)
        ) {
        struct p7r_stack_metamark *mark = stack_page_iterator;
        list_add_tail(&(mark->linkable), &(provider->pages));
        mark->provider = provider;
        mark->n_bytes_page = n_bytes_page;
        mark->red_zone_addr = stack_page_iterator + n_bytes_page;
        mark->raw_content_addr = stack_page_iterator + n_bytes_page * 2;
        mprotect(mark->red_zone_addr, n_bytes_page, PROT_NONE);
    }

    return provider;
}

static
struct p7r_stack_page_provider *p7r_stack_page_provider_ruin(struct p7r_stack_page_provider *provider) {
    if (provider->zone) {
        munmap(provider->zone, provider->total_size);
    }
    return provider;
}

static
struct p7r_stack_page_provider *p7r_stack_page_provider_new(
        uint32_t type,
        uint32_t n_pages_capacity, 
        uint32_t n_pages_stack, 
        uint32_t n_bytes_page,
        struct p7r_stack_allocator *parent
    ) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    struct p7r_stack_page_provider *provider = scraft_allocate(allocator, sizeof(struct p7r_stack_page_provider));

    if (provider) {
        if (unlikely(p7r_stack_page_provider_init(provider, type, n_pages_capacity, n_pages_stack, n_bytes_page, parent) == NULL)) {
            scraft_deallocate(allocator, provider);
            return NULL;
        }
    }

    return provider;
}

static
void p7r_stack_page_provider_delete(struct p7r_stack_page_provider *provider) {
    __auto_type allocator = p7r_root_alloc_get_proxy();
    scraft_deallocate(allocator, p7r_stack_page_provider_ruin(provider));
}

static inline
void p7r_stack_page_slaver_init(struct p7r_stack_page_slaver *slaver) {
    slaver->n_slaves = 0;
    init_list_head(&(slaver->slaves));
}

struct p7r_stack_metamark *p7r_stack_page_allocate(struct p7r_stack_page_provider *provider) {
    struct p7r_stack_metamark *target = NULL;

    if (provider->size == 0) {
        return NULL;
    }

    list_ctl_t *target_link = provider->pages.next;
    list_del(target_link);
    provider->size -= provider->parent->properties.n_pages_stack_total;
    target = container_of(target_link, struct p7r_stack_metamark, linkable);
    target->n_bytes_page = provider->n_bytes_page;

    return target;
}

static
int p7r_slave_free_adjust(struct p7r_stack_page_provider *slave) {
    uint32_t dummy_size = slave->size;
    int dying = 0;

    if (!dummy_size) {
        struct p7r_stack_page_provider *compared = container_of(slave->parent->slaves.slaves.next, struct p7r_stack_page_provider, linkable);
        if (compared != slave) {
            list_del(&(slave->linkable));
            list_add_head(&(slave->linkable), &(slave->parent->slaves.slaves));
        }
    }

    dummy_size += slave->parent->properties.n_pages_stack_total;

    if (dummy_size == slave->capacity) {
        list_del(&(slave->linkable));
        dying = 1;
    }

    return dying;
}

static
int p7r_slave_allocation_adjust(struct p7r_stack_page_provider *slave) {
    if (slave->size == 0) {
        list_del(&(slave->linkable));
        list_add_tail(&(slave->linkable), &(slave->parent->slaves.slaves));
    }
}

void p7r_stack_page_free(struct p7r_stack_metamark *mark) {
    struct p7r_stack_page_provider *provider = mark->provider;

    // we don't care anything about sequence of stacks for they are all the same
    list_add_head(&(mark->linkable), &(provider->pages));

    if (provider->type == P7R_STACK_ALLOCATOR_SLAVE) {
        if (p7r_slave_free_adjust(provider)) {
            provider->parent->slaves.n_slaves--;
            p7r_stack_page_provider_delete(provider);
            return;
        }
    }
    provider->size += provider->parent->properties.n_pages_stack_total;
}

// TODO refactor: remove hard-coded pipeline

struct p7r_stack_metamark *p7r_stack_page_allocate_fallback(struct p7r_stack_allocator *allocator) {
    struct p7r_stack_metamark *result = NULL;
    struct p7r_stack_page_slaver *slaver = &(allocator->slaves);
    struct p7r_stack_page_provider *target_slave = NULL;

    list_ctl_t *slave_iterator;
    list_foreach(slave_iterator, &(allocator->slaves.slaves)) {
        struct p7r_stack_page_provider *slave = container_of(slave_iterator, struct p7r_stack_page_provider, linkable);
        if (slave->size) {
            target_slave = slave;
            break;
        }
    }

    if (!target_slave) {
        target_slave = 
            p7r_stack_page_provider_new(
                    P7R_STACK_ALLOCATOR_SLAVE, 
                    allocator->properties.n_pages_slave, 
                    allocator->properties.n_pages_stack_total, 
                    allocator->properties.n_bytes_page,
                    allocator
            );
        list_add_head(&(target_slave->linkable), &(allocator->slaves.slaves));
        allocator->slaves.n_slaves++;
    }
    result = p7r_stack_page_allocate(target_slave);
    p7r_slave_allocation_adjust(target_slave);      // demote an empty slave

    return result;
}

static
struct p7r_stack_metamark *p7r_stack_page_allocate_long_term(struct p7r_stack_allocator *allocator) {
    struct p7r_stack_metamark *result = NULL;
    struct p7r_stack_page_provider *self = &(allocator->long_term);

    if ((result = p7r_stack_page_allocate(self)) == NULL)
        result = p7r_stack_page_allocate_fallback(allocator);

    return result;
}

static
struct p7r_stack_metamark *p7r_stack_page_allocate_short_term(struct p7r_stack_allocator *allocator) {
    struct p7r_stack_metamark *result = NULL;
    struct p7r_stack_page_provider *self = &(allocator->short_term);

    if ((result = p7r_stack_page_allocate(self)) == NULL)
        result = p7r_stack_page_allocate_long_term(allocator);

    return result;
}

static
struct p7r_stack_allocator_config p7r_stack_allocator_config_adjust(struct p7r_stack_allocator_config *config_) {
    __auto_type config = *config_;
    config.n_pages_stack_user = config.n_pages_stack_total - 2;
#define adjust_capacity(capacity_) \
    do { \
        if ((capacity_) % config.n_pages_stack_total) \
            (capacity_) = (capacity_) - (capacity_) % config.n_pages_stack_total + config.n_pages_stack_total; \
    } while (0)
    adjust_capacity(config.n_pages_short_term);
    adjust_capacity(config.n_pages_long_term);
    adjust_capacity(config.n_pages_slave);
    return config;
#undef adjust_capacity
}

double p7r_stack_allocator_usage(struct p7r_stack_allocator *allocator) {
    return ((double) allocator->short_term.size) / ((double) allocator->short_term.capacity);
}

struct p7r_stack_allocator *p7r_stack_allocator_init(struct p7r_stack_allocator *allocator, struct p7r_stack_allocator_config config) {
    allocator->properties = p7r_stack_allocator_config_adjust(&config);
    p7r_stack_page_slaver_init(&(allocator->slaves));

    p7r_stack_page_provider_init(
            &(allocator->long_term),
            P7R_STACK_ALLOCATOR_MASTER, 
            allocator->properties.n_pages_long_term, 
            allocator->properties.n_pages_stack_total, 
            allocator->properties.n_bytes_page, 
            allocator
    );
    p7r_stack_page_provider_init(
            &(allocator->short_term),
            P7R_STACK_ALLOCATOR_MASTER, 
            allocator->properties.n_pages_short_term, 
            allocator->properties.n_pages_stack_total, 
            allocator->properties.n_bytes_page, 
            allocator
    );

    void safe_deleter(struct p7r_stack_page_provider *provider) {
        if(provider)
            p7r_stack_page_provider_ruin(provider);
    }

    if ((allocator->long_term.zone == NULL) || (allocator->short_term.zone == NULL)) {
        safe_deleter(&(allocator->long_term));
        safe_deleter(&(allocator->short_term));
        return NULL;
    }

    return allocator;
}

void p7r_stack_allocator_ruin(struct p7r_stack_allocator *allocator) {
    p7r_stack_page_provider_ruin(&(allocator->long_term));
    p7r_stack_page_provider_ruin(&(allocator->short_term));

    list_ctl_t *iterator_forward, *iterator_actual;
    list_foreach_remove(iterator_forward, &(allocator->slaves.slaves), iterator_actual) {
        list_del(iterator_actual);
        struct p7r_stack_page_provider *slave = container_of(iterator_actual, struct p7r_stack_page_provider, linkable);
        p7r_stack_page_provider_delete(slave);
    }
}

struct p7r_stack_metamark *p7r_stack_allocate(int type, struct p7r_stack_allocator *allocator) {
    struct p7r_stack_metamark *result;

    switch (type) {
        case P7R_STACK_SOURCE_DEFAULT:
            result = p7r_stack_page_allocate_long_term(allocator);
            break;
        case P7R_STACK_SOURCE_SHORT_TERM:
            result = p7r_stack_page_allocate_short_term(allocator);
            break;
        default:
            result = NULL;
    }

    return result;
}

void p7r_stack_free(struct p7r_stack_metamark *mark) {
    p7r_stack_page_free(mark);
}
