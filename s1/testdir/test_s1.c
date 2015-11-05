#include    <stdio.h>
#include    <stdlib.h>
#include    <errno.h>
#include    "../../include/model_alloc.h"
#include    "../../include/model_thread.h"
#include    "../s1_dlbase.h"
#include    "../s1_alloc.h"
#include    "../s1_mutex.h"
#include    "../../p7/libp7.h"
#include    "../../p7/p7_alloc.h"

struct s1_dlmgr *mgr_global;
struct p7_spinlock spin, *spin_base;

void testfunc(void *num) {
    if (!num)
        abort();
    struct s1_dlsym sym = s1_dllib_symbol(*mgr_global, "libtestdl.so", "printmsg");
    while (1) {
        if (sym.symbol != NULL) {
            sym = s1_call_guard(sym);
            ((void (*)(void)) sym.symbol)();
            sym = s1_call_unguard(sym);
        } else {
            write(STDOUT_FILENO, "deprecated\n", sizeof("deprecated\n"));
        }
        p7_coro_yield();
    }
}

int main(int argc, char *argv[]) {
    __auto_type allocator_s1 = s1_root_alloc_get_allocator();
    __auto_type allocator_p7 = p7_root_alloc_get_allocator();
    allocator_s1->allocator_.closure_ = allocator_p7->allocator_.closure_ = malloc;
    allocator_s1->deallocator_.closure_ = allocator_p7->deallocator_.closure_ = free;
    allocator_s1->reallocator_.closure_ = allocator_p7->reallocator_.closure_ = realloc;
    __auto_type mutex_model_s1 = s1_root_mutex_get_mutex_model();
    void adapted_mutex_lock(void *arg) {
        struct p7_spinlock *spin = arg;
        p7_spinlock_lock(spin);
    }
    void adapted_mutex_unlock(void *arg) {
        struct p7_spinlock *spin = arg;
        p7_spinlock_unlock(spin);
    }
    mutex_model_s1->lock_.plain_ptr_ = adapted_mutex_lock;
    mutex_model_s1->unlock_.plain_ptr_ = adapted_mutex_unlock;
    p7_init(atoi(argv[1]), NULL, NULL);
    spin_base = &spin;
    p7_spinlock_init(spin_base, 400);
    void *spin_ctor(void *unused) { return spin_base; }
    void spin_dtor(void *unused) { }
    struct s1_dlmgr mgr = s1_dllib_create(4, spin_ctor, spin_dtor, 1, NULL);
    mgr_global = &mgr;
    s1_dllib_load(mgr, "libtestdl.so", "./libtestdl.so.1");
    uint32_t cnt = 1;
    p7_coro_create(testfunc, (void *) cnt, 4096 * 4096);
    while (1) {
        cnt++;
        p7_coro_yield();
        if (cnt == 1<<16) {
            write(STDOUT_FILENO, "reloading\n", sizeof("reloading\n"));
            if (s1_dllib_reload(mgr, "libtestdl.so", "./libtestdl.so.2") == -1) {
                exit(1);
            }
            write(STDOUT_FILENO, "reloaded\n", sizeof("reloaded\n"));
            p7_coro_create(testfunc, (void *) cnt, 4096 * 4096);
            while (1) p7_coro_yield();
        }
    }
    return 0;
}
