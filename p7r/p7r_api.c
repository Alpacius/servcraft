#include    "./p7r_api.h"
#include    "./p7r_root_alloc.h"


static
struct p7r_poolized_meta {
    int startup_channel[2];
} meta_singleton;


static
void p7r_poolized_main_entrance(void *argument) {
    struct p7r_poolized_meta *meta = argument;
    struct p7r_delegation channel_hint;
    for (;;) {
        channel_hint = p7r_delegate(P7R_DELEGATION_READ, meta->startup_channel[0]);
        // XXX currently do nothing - 'tis wrong for fd under LT mode, so we write nothing to startup channel for now
    }
}

int p7r_poolize(struct p7r_config config) {
    if (pipe(meta_singleton.startup_channel) == -1)
        return -1;
    int ret = p7r_init(config);
    if (ret < 0)
        return close(meta_singleton.startup_channel[0]), close(meta_singleton.startup_channel[1]), ret;
    return p7r_poolized_main_entrance(&meta_singleton), 0;
}
