#ifndef     ROOT_ALLOC_PREFIX
#error      "missing root allocator prefix"
#endif

#define     local_root_allocator        cat2_(ROOT_ALLOC_PREFIX, _root_allocator)

static struct scraft_model_alloc local_root_allocator;

struct scraft_model_alloc cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_proxy(void)) {
    return local_root_allocator;
}

struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_allocator(void)) {
    return &local_root_allocator;
}

struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_ruin(void)) {
    return &local_root_allocator;
}
