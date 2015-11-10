#include    <stdlib.h>
#include    <string.h>
#include    <stdint.h>
#include    <dlfcn.h>
#include    "./s1_hashdic.h"
#include    "./s1_dlbase.h"
#include    "./s1_root_alloc.h"
#include    "./s1_root_mutex.h"
#include    "../include/util_list.h"
#include    "../include/miscutils.h"
#include    "../include/model_alloc.h"
#include    "../include/model_thread.h"


static
struct s1_dlwrap *s1_dlwrap_init(struct s1_dlwrap *wrap, const char *path) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    wrap->path = strcpy( ({ char *p = scraft_allocate(allocator, sizeof(char) * strlen(path)); if (unlikely(p == NULL)) return NULL; p; }), path );
    (wrap->invcnt = 0), (wrap->base = NULL);
    scraft_deallocate( allocator, ({ if (likely((wrap->dlhandle = dlopen(path, RTLD_LAZY)) != NULL)) goto success; wrap->path; }) );
    wrap = NULL;
success:
    return wrap;
}

static
struct s1_dlwrap *s1_dlwrap_ruin(struct s1_dlwrap *wrap) {
    scraft_deallocate(s1_root_alloc_get_proxy(), wrap->path);
    dlclose(wrap->dlhandle);
    return wrap;
}

static
int alias_compare(const void *lhs, const void *rhs) {
    return strncmp((const char *) lhs, (const char *) rhs, FILENAME_MAX);
}

static
struct s1_dlbase *s1_dlbase_init(struct s1_dlbase *base, const char *alias, const char *initial_path, void *mutex, void (*mutex_dtor)(void *)) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    struct s1_dlwrap *wrap = s1_dlwrap_init( ({ struct s1_dlwrap *wrap_ = scraft_allocate(allocator, sizeof(struct s1_dlwrap)); if (unlikely(wrap_ == NULL)) return NULL; wrap_; }), initial_path );
    if (unlikely(wrap == NULL))
        return NULL;
    (base->loading = NULL), (base->stable = wrap);
    strncpy(base->alias, alias, FILENAME_MAX);
    (base->deprecated.deprecated_alivecnt = 0), (base->deprecated.deprecated_mutex = mutex), (base->deprecated.deprecated_mutex_dtor = mutex_dtor);
    init_list_head(&(base->deprecated.deprecated_set));
    return base;
}

static
struct s1_dlbase *s1_dlbase_ruin(struct s1_dlbase *base) {
    __auto_type allocator = s1_root_alloc_get_proxy();
    list_ctl_t *p, *t, *h = &(base->deprecated.deprecated_set), l;
    init_list_head(&l);
    __auto_type mutex_model = s1_root_mutex_get_proxy();
    scraft_mutex_lock(mutex_model, base->deprecated.deprecated_mutex);
    list_foreach_remove(p, h, t) {
        list_del(t);
        list_add_tail(t, &l);
    }
    scraft_mutex_unlock(mutex_model, base->deprecated.deprecated_mutex);
    scraft_deallocate(allocator, s1_dlwrap_ruin(base->stable));
    if (base->loading != NULL)
        scraft_deallocate(allocator, s1_dlwrap_ruin(base->loading));
    list_foreach_remove(p, &l, t) {
        list_del(t);
        scraft_deallocate(allocator, s1_dlwrap_ruin(container_of(t, struct s1_dlwrap, lctl)));
    }
    base->deprecated.deprecated_mutex_dtor(base->deprecated.deprecated_mutex);
    return base;
}

// hashval dtor
static
void s1_dlbase_destroy(void *arg) {
    scraft_deallocate(s1_root_alloc_get_proxy(), s1_dlbase_ruin((struct s1_dlbase *) arg));
}

struct s1_dlmgr s1_dllib_create(uint32_t cap, void *(*mutex_ctor)(void *), void (*mutex_dtor)(void *), int use_default_arg, void *default_arg) {
    struct s1_dlmgr mgr;
    if (likely((mgr.dic = s1_dic_init(s1_hasher_cstring_djb, NULL, s1_dlbase_destroy, alias_compare, cap)) != NULL))
        (mgr.mutex_meta.use_default_arg = use_default_arg), (mgr.mutex_meta.default_arg = default_arg), (mgr.mutex_meta.mutex_ctor = mutex_ctor), (mgr.mutex_meta.mutex_dtor = mutex_dtor);
    return mgr;
}

struct s1_dlmgr s1_dllib_ruin(struct s1_dlmgr mgr) {
    s1_dic_ruin(mgr.dic);
    return mgr;
}

int s1_dllib_load(struct s1_dlmgr mgr, const char *alias, const char *path) {
    int ret = -1;
    if (likely(s1_dic_fetch(mgr.dic, alias) == NULL)) {
        __auto_type allocator = s1_root_alloc_get_proxy();
        struct s1_dlbase *base =
            s1_dlbase_init(
                    ({ struct s1_dlbase *base_ = scraft_allocate(allocator, sizeof(struct s1_dlbase)); if (unlikely(base_ == NULL)) goto load_outbreak; base_; }),
                    alias, 
                    path, 
                    mgr.mutex_meta.mutex_ctor(mgr.mutex_meta.default_arg), 
                    mgr.mutex_meta.mutex_dtor
            );
        s1_dic_insert(mgr.dic, base->alias, base);
        ret = 0;
    }
load_outbreak:
    return ret;
}

int s1_dllib_unload(struct s1_dlmgr mgr, const char *alias) {
    int ret = -1;
    if (likely(s1_dic_fetch(mgr.dic, alias) != NULL))
        s1_dic_delete(mgr.dic, alias);
    return ret;
}

int s1_dllib_reload(struct s1_dlmgr mgr, const char *alias, const char *path) {
    int ret = -1;
    struct s1_dlbase *base;
    if (likely((base = s1_dic_fetch(mgr.dic, alias)) != NULL)) {
        __auto_type allocator = s1_root_alloc_get_proxy();
        // XXX 'Tis wrong
        struct s1_dlwrap *wrap = s1_dlwrap_init(({ struct s1_dlwrap *wrap_ = scraft_allocate(allocator, sizeof(struct s1_dlwrap)); if (unlikely(wrap_ == NULL)) goto reload_outbreak; wrap_; }), path);
        if (unlikely(wrap->dlhandle == NULL)) {
            scraft_deallocate(allocator, wrap);
            return ret;
        }
        __auto_type mutex_model = s1_root_mutex_get_proxy();
        base->loading = wrap;
        base->stable->base = base;
        scraft_mutex_lock(mutex_model, base->deprecated.deprecated_mutex);
        base->deprecated.deprecated_alivecnt += (__atomic_load_n(&(base->stable->invcnt), __ATOMIC_ACQUIRE) > 0);
        list_add_tail(&(base->stable->lctl), &(base->deprecated.deprecated_set));
        scraft_mutex_unlock(mutex_model, base->deprecated.deprecated_mutex);
        (__atomic_store_n(&(base->stable), wrap, __ATOMIC_RELEASE)), (base->loading = NULL);
        ret = 0;
    }
reload_outbreak:
    return ret;
}

struct s1_dlsym s1_dllib_symbol(struct s1_dlmgr mgr, const char *alias, const char *symbol) {
    struct s1_dlsym sym = (struct s1_dlsym) { NULL, NULL };
    struct s1_dlbase *base;
    if (likely((base = s1_dic_fetch(mgr.dic, alias)) != NULL))
        (sym.symbol = dlsym(base->stable->dlhandle, symbol)), (sym.private_ref_ = base->stable);
    return sym;
}

struct s1_dlsym s1_call_guard(struct s1_dlsym sym) {
    struct s1_dlwrap *wrap = sym.private_ref_;
    __atomic_add_fetch(&(wrap->invcnt), 1, __ATOMIC_RELEASE);
    return sym;
}

struct s1_dlsym s1_call_unguard(struct s1_dlsym sym) {
    struct s1_dlwrap *wrap = sym.private_ref_;
    __auto_type mutex_model = s1_root_mutex_get_proxy();
    __auto_type allocator = s1_root_alloc_get_proxy();
    list_ctl_t l;
    if (unlikely((__atomic_sub_fetch(&(wrap->invcnt), 1, __ATOMIC_RELEASE) < 1) && (wrap->base != NULL))) {
        struct s1_dlbase *base = wrap->base;
        if (unlikely(__atomic_sub_fetch(&(wrap->base->deprecated.deprecated_alivecnt), 1, __ATOMIC_RELEASE) < 1)) {
            init_list_head(&l);
            scraft_mutex_lock(mutex_model, wrap->base->deprecated.deprecated_mutex);
            list_ctl_t *p, *t, *h = &(wrap->base->deprecated.deprecated_set);
            list_foreach_remove(p, h, t) {
                list_del(t);
                list_add_tail(t, &l);
            }
            scraft_mutex_unlock(mutex_model, wrap->base->deprecated.deprecated_mutex);
            list_foreach_remove(p, &l, t) {
                list_del(t);
                scraft_deallocate(allocator, s1_dlwrap_ruin(container_of(t, struct s1_dlwrap, lctl)));
            }
        }
        (sym.symbol = NULL), (sym.private_ref_ = __atomic_load_n(&(base->stable), __ATOMIC_ACQUIRE));       // XXX ready for reloading symbol: call s1_dlsym_reload later
    }
    return sym;
}

struct s1_dlsym s1_dlsym_reload(struct s1_dlsym sym, const char *symbol) {
    struct s1_dlwrap *wrap = sym.private_ref_;
    sym.symbol = dlsym(wrap->dlhandle, symbol);
    return sym;
}
