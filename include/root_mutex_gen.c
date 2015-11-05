#ifndef     ROOT_MUTEX_PREFIX
#error      "missing root mutex_model prefix"
#endif

#define     local_root_mutex_model        cat2_(ROOT_MUTEX_PREFIX, _root_mutex_model)

static struct scraft_model_mutex local_root_mutex_model;

struct scraft_model_mutex cat2_(ROOT_MUTEX_PREFIX, _root_mutex_get_proxy(void)) {
    return local_root_mutex_model;
}

struct scraft_model_mutex * cat2_(ROOT_MUTEX_PREFIX, _root_mutex_get_mutex_model(void)) {
    return &local_root_mutex_model;
}

struct scraft_model_mutex * cat2_(ROOT_MUTEX_PREFIX, _root_mutex_ruin(void)) {
    return &local_root_mutex_model;
}

#undef      local_root_mutex_model
