#ifndef     LIBS1_H_
#define     LIBS1_H_

#include    "./s1_dlbase.h"

struct s1_dlmgr s1_dllib_create(uint32_t cap, void *(*mutex_ctor)(void *), void (*mutex_dtor)(void *), int use_default_arg, void *default_arg);
struct s1_dlmgr s1_dllib_ruin(struct s1_dlmgr mgr);
int s1_dllib_load(struct s1_dlmgr mgr, const char *alias, const char *path);
int s1_dllib_unload(struct s1_dlmgr mgr, const char *alias);
int s1_dllib_reload(struct s1_dlmgr mgr, const char *alias, const char *path);
struct s1_dlsym s1_dllib_symbol(struct s1_dlmgr mgr, const char *alias, const char *symbol);
struct s1_dlsym s1_call_guard(struct s1_dlsym sym);
struct s1_dlsym s1_call_unguard(struct s1_dlsym sym);
struct s1_dlsym s1_dlsym_reload(struct s1_dlsym sym, const char *symbol);

#endif      // LIBS1_H_
