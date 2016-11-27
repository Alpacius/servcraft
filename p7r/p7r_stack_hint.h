#ifndef     P7R_STACK_HINT_H_
#define     P7R_STACK_HINT_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"
#include    "./p7r_stack_allocator.h"

struct p7r_hint_execution_time_stat {
    double average, recent;
    uint32_t measure_recent;
};

struct p7r_hint_failure_stat {
    uint32_t measure_limit, measure_total, measure_failed;
};

struct p7r_stack_hint_config {
    struct p7r_hint_execution_time_stat execution_time_stat;
    struct p7r_hint_failure_stat failure_stat;
};

struct p7r_stack_hint {
    struct {
        void (*solid_entrance)(void *);
        const char *name;
    } key;
    struct p7r_hint_execution_time_stat execution_time_stat;
    struct p7r_hint_failure_stat failure_stat;
    uint8_t policy;
    struct scraft_hashkey hashable;
};

#define     P7R_STACK_POLICY_DEFAULT        P7R_STACK_SOURCE_DEFAULT
#define     P7R_STACK_POLICY_EDEN           P7R_STACK_SOURCE_SHORT_TERM

struct p7r_stack_hint *p7r_stack_hint_init_by_name(struct p7r_stack_hint *hint, const char *name, const struct p7r_stack_hint_config *config);
struct p7r_stack_hint *p7r_stack_hint_init_by_entrance(struct p7r_stack_hint *hint, void (*entrance)(void *), const struct p7r_stack_hint_config *config);
struct p7r_stack_hint *p7r_stack_hint_new_from_name(const char *name, struct p7r_stack_hint_config config);
struct p7r_stack_hint *p7r_stack_hint_new_from_entrance(void (*entrance)(void *), struct p7r_stack_hint_config config);

struct p7r_stack_metamark *p7r_stack_allocate_hintless(struct p7r_stack_allocator *allocator, uint8_t policy);

#define p7r_stack_hint_init(hint_, arg_, config_) \
    _Generic((arg_), char *: p7r_stack_hint_init_by_name, void (*)(void *): p7r_stack_hint_init_by_entrance)((hint_), (arg_), (config_))

#define p7r_stack_hint_new(arg_, config_) \
    _Generic((arg_), char *: p7r_stack_hint_new_from_name, void (*)(void *): p7r_stack_hint_new_from_entrance)((hint_), (arg_), (config_))

struct p7r_stack_metamark *p7r_stack_allocate_with_hint(struct p7r_stack_allocator *allocator, struct p7r_stack_hint *hint);

uint64_t p7r_stack_hint_entry_hash(struct scraft_hashkey *key);
int p7r_stack_hint_entry_destroy(struct scraft_hashkey *key);
int p7r_stack_hint_entry_compare(struct scraft_hashkey *lhs_, struct scraft_hashkey *rhs_);

#endif      // P7R_STACK_HINT_H_
