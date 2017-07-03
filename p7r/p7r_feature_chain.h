#ifndef     P7R_FEATURE_CHAIN_H_
#define     P7R_FEATURE_CHAIN_H_

#include    "./p7r_stdc_common.h"
#include    "./p7r_linux_common.h"
#include    "./p7r_scraft_common.h"
#include    "./p7r_uthread_def.h"


struct p7r_feature_desc {
    const char *name;
    uint32_t priority;
    void (*initializer)(const struct p7r_config *config);
    void (*finalizer)(void);
    list_ctl_t lctl;
};

struct p7r_feature_chain {
    list_ctl_t chain;
};


static inline
void p7r_feature_chain_init(struct p7r_feature_chain *chain, struct p7r_feature_desc *features, uint32_t n_features) {
    int feature_comparator(const void *lhs_, const void *rhs_) {
        const struct p7r_feature_desc *lhs = lhs_, *rhs = rhs_;
        return (lhs->priority < rhs->priority) ? -1 : ((lhs->priority > rhs->priority) ? 1 : 0);
    }
    struct p7r_feature_desc *features_sorted[n_features];
    for (uint32_t feature_index = 0; feature_index < n_features; feature_index++)
        features_sorted[feature_index] = &(features[feature_index]);
    qsort(features_sorted, n_features, sizeof(struct p7r_feature_desc *), feature_comparator);
    init_list_head(&(chain->chain));
    for (uint32_t feature_index = 0; feature_index < n_features; feature_index++)
        list_add_tail(&(features_sorted[feature_index]->lctl), &(chain->chain));
}

static inline
void p7r_features_init(const struct p7r_feature_chain *features, const struct p7r_config *config) {
    list_ctl_t *iter;
    list_foreach(iter, &(features->chain)) {
        struct p7r_feature_desc *feature = container_of(iter, struct p7r_feature_desc, lctl);
        if (feature->initializer)
            feature->initializer(config);
    }
}

static inline
void p7r_features_ruin(const struct p7r_feature_chain *features) {
    list_ctl_t *iter;
    for (iter = &(features->chain.prev); iter != &(features->chain); iter = iter->prev) {
        struct p7r_feature_desc *feature = container_of(iter, struct p7r_feature_desc, lctl);
        if (feature->finalizer)
            feature->finalizer();
    }
}


#define p7r_feature_chain_static_init(chain_, features_) \
    do { \
        uint32_t n_features = sizeof(features_) / sizeof(struct p7r_feature_desc); \
        p7r_feature_chain_init((chain_), &(features_), n_features); \
    } while (0)


#endif      // P7R_FEATURE_CHAIN_H_
