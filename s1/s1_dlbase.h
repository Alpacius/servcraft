#ifndef     S1_DLBASE_H_
#define     S1_DLBASE_H_

#include    <stdio.h>
#include    <stdint.h>
#include    <stdlib.h>
#include    <string.h>
#include    "./s1_hashdic.h"
#include    "../include/util_list.h"
#include    "../include/miscutils.h"

struct s1_dlbase;

struct s1_dlwrap {
    char *path;
    void *dlhandle;
    uint64_t invcnt;
    struct s1_dlbase *base;
    list_ctl_t lctl;
};

struct s1_dlbase {
    char alias[FILENAME_MAX];
    struct s1_dlwrap *loading, *stable;
    struct {
        list_ctl_t deprecated_set;
        uint64_t deprecated_alivecnt;
        void *deprecated_mutex;
        void (*deprecated_mutex_dtor)(void *);
    } deprecated;
};

struct s1_dlmgr {
    struct s1_dic *dic;
    struct {
        void (*mutex_dtor)(void *);
        void *(*mutex_ctor)(void *);
        void *default_arg;
        int use_default_arg;
    } mutex_meta;
};

struct s1_dlsym {
    void *symbol, *private_ref_;
};

struct s1_dlmgr s1_dllib_create(uint32_t cap, void *(*mutex_ctor)(void *), void (*mutex_dtor)(void *), int use_default_arg, void *default_arg);
struct s1_dlmgr s1_dllib_ruin(struct s1_dlmgr mgr);
int s1_dllib_load(struct s1_dlmgr mgr, const char *alias, const char *path);
int s1_dllib_unload(struct s1_dlmgr mgr, const char *alias);
int s1_dllib_reload(struct s1_dlmgr mgr, const char *alias, const char *path);
struct s1_dlsym s1_dllib_symbol(struct s1_dlmgr mgr, const char *alias, const char *symbol);
struct s1_dlsym s1_call_guard(struct s1_dlsym sym);
struct s1_dlsym s1_call_unguard(struct s1_dlsym sym);
struct s1_dlsym s1_dlsym_reload(struct s1_dlsym sym, const char *symbol);

#endif      // S1_DLBASE_H_
