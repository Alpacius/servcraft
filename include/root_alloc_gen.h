#pragma once

#ifndef     ROOT_ALLOC_PREFIX
#error      "missing root allocator prefix"
#endif

#include    "miscutils.h"
#include    "model_alloc.h"

struct scraft_model_alloc cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_proxy(void));
struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_get_allocator(void));
struct scraft_model_alloc * cat2_(ROOT_ALLOC_PREFIX, _root_alloc_ruin(void));

