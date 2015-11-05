#pragma once

#ifndef     ROOT_MUTEX_PREFIX
#error      "missing root mutex_model prefix"
#endif

#include    "miscutils.h"
#include    "model_thread.h"

struct scraft_model_mutex cat2_(ROOT_MUTEX_PREFIX, _root_mutex_get_proxy(void));
struct scraft_model_mutex * cat2_(ROOT_MUTEX_PREFIX, _root_mutex_get_mutex_model(void));
struct scraft_model_mutex * cat2_(ROOT_MUTEX_PREFIX, _root_mutex_ruin(void));

