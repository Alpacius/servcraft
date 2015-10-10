#pragma once

#ifndef     ROOT_ALLOC_PREFIX
#error      "missing root allocator prefix"
#endif

#include    "miscutils.h"
#include    "model_alloc.h"

struct scraft_model_alloc cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_proxy(void));
struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_allocator(void));
struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_ruin(void));

#define local_root_alloc_get_proxy \
    cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_proxy)
#define local_root_alloc_get_allocator \
    cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_allocator)
#define local_root_alloc_ruin \
    cat2_(ROOT_ALLOC_PREFIX, _root_alloc_ruin)
