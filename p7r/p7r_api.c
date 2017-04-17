#include    "./p7r_api.h"
#include    "./p7r_root_alloc.h"


static
struct p7r_poolized_meta {
    pthread_t main_thread;
    int pool_alive;
    int startup_channel[2];
    pthread_spinlock_t *foreign_request_mutex;
} meta_singleton = { .pool_alive = 0 };


static
void p7r_poolized_main_entrance(void *argument) {
    struct p7r_poolized_meta *meta = argument;
    struct p7r_delegation channel_hint;
    for (;;) {
        channel_hint = p7r_delegate(P7R_DELEGATION_READ, meta->startup_channel[0]);
        // XXX currently do nothing - 'tis wrong for fd under LT mode, so we write nothing to startup channel for now
    }
}

static
int p7r_poolize_(struct p7r_config config) {
    int ret = p7r_init(config);
    __auto_type allocator = p7r_root_alloc_get_proxy();
    if (ret < 0)
        return -1;
    uint32_t n_carriers = p7r_n_carriers();
    if (unlikely((meta_singleton.foreign_request_mutex = scraft_allocate(allocator, sizeof(pthread_spinlock_t) * n_carriers)) == NULL))
        return -1;
    for (uint32_t target_index = 0; target_index < n_carriers; target_index++)
        pthread_spin_init(&(meta_singleton.foreign_request_mutex[target_index]), PTHREAD_PROCESS_PRIVATE);
    if (pipe(meta_singleton.startup_channel) == -1) {
        scraft_deallocate(allocator, (void *) meta_singleton.foreign_request_mutex);
        return -1;
    }
    __atomic_store_n(&(meta_singleton.pool_alive), 1, __ATOMIC_RELAXED);
    return p7r_poolized_main_entrance(&meta_singleton), 0;
}

static
void *p7r_poolized_main_thread(void *config_argument) {
    if (unlikely(p7r_poolize_(*((struct p7r_config *) config_argument)) < 0))
        __atomic_store_n(&(meta_singleton.pool_alive), -1, __ATOMIC_RELAXED);
    return NULL;
}

int p7r_poolization_status(void) {
    return __atomic_load_n(&(meta_singleton.pool_alive), __ATOMIC_RELAXED);
}

int p7r_poolize(struct p7r_config config) {
    pthread_attr_t detach_attr;
    pthread_attr_init(&detach_attr);
    pthread_attr_setdetachstate(&detach_attr, PTHREAD_CREATE_DETACHED);
    return pthread_create(&(meta_singleton.main_thread), &detach_attr, p7r_poolized_main_thread, &config);
}

int p7r_execute(void (*entrance)(void *), void *argument, void (*dtor)(void *)) {
    uint32_t target = balanced_target_carrier();
    int ret = -1;
    pthread_spin_lock(&(meta_singleton.foreign_request_mutex[target]));
    {
        ret = p7r_uthread_create_foreign(target, entrance, argument, dtor);
    }
    pthread_spin_unlock(&(meta_singleton.foreign_request_mutex[target]));
    return ret;
}
