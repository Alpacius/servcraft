#include    <stdio.h>
#include    <stdlib.h>
#include    <string.h>
#include    "../s1_root_alloc.h"
#include    "../s1_hashdic.h"

int compare_cstring(const void * lhs, const void *rhs) {
    const char *str_lhs = (const char *) lhs, *str_rhs = (const char *) rhs;
    return strcmp(str_lhs, str_rhs);
}

int main(void) {
    struct scraft_model_alloc *root_allocator = s1_root_alloc_get_allocator();
    (root_allocator->allocator_.closure_ = malloc), (root_allocator->deallocator_.closure_ = free);
    struct s1_dic *dic = s1_dic_init(s1_hasher_cstring_djb, NULL, NULL, compare_cstring, 31);
    s1_dic_insert(dic, "key1", "value1");
    s1_dic_insert(dic, "key2", "value2");
    const char *value = s1_dic_fetch(dic, "key1");
    printf("value = %s(%p) dic status = %u/%u\n", value, value, dic->size, dic->cap);
    s1_dic_delete(dic, "key1");
    value = s1_dic_fetch(dic, "key1");
    printf("value = %p dic status = %u/%u\n", value, dic->size, dic->cap);
    s1_dic_ruin(dic);
    return 0;
}
