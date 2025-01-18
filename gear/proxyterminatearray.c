#include <stdbool.h>
#include <glib.h>

#include "util.h"

static GPtrArray *proxy_terminate_array = NULL;
static pthread_mutex_t proxy_terminate_array_lock;

void proxy_terminate_array_create(void) {
    pthread_mutexattr_t mtx_attr;
    
    if(proxy_terminate_array==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&proxy_terminate_array_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        proxy_terminate_array = g_ptr_array_new_full(1, (GDestroyNotify)free);
    }
}

void proxy_terminate_array_destroy(void) {
    if(proxy_terminate_array!=NULL) {
        g_ptr_array_free(proxy_terminate_array, true);
        proxy_terminate_array = NULL;
        pthread_mutex_destroy(&proxy_terminate_array_lock);
    }
}

void proxy_terminate_array_set(const char *proxy_name) {
    pthread_mutex_lock(&proxy_terminate_array_lock);
    if(!g_ptr_array_find_with_equal_func(proxy_terminate_array, proxy_name, (GEqualFunc)str_equal, NULL)) {
        g_ptr_array_add(proxy_terminate_array, strdup(proxy_name));
    }
    pthread_mutex_unlock(&proxy_terminate_array_lock);
}

void proxy_terminate_array_drop(const char *proxy_name) {
    guint idx;

    pthread_mutex_lock(&proxy_terminate_array_lock);
    if(g_ptr_array_find_with_equal_func(proxy_terminate_array, proxy_name, (GEqualFunc)str_equal, &idx)) {
        g_ptr_array_remove_index(proxy_terminate_array, idx);
    }
    pthread_mutex_unlock(&proxy_terminate_array_lock);
}

bool proxy_terminate_array_exists(const char *proxy_name) {
    bool exists;

    pthread_mutex_lock(&proxy_terminate_array_lock);
    exists = g_ptr_array_find_with_equal_func(proxy_terminate_array, proxy_name, (GEqualFunc)str_equal, NULL);
    pthread_mutex_unlock(&proxy_terminate_array_lock);
    return exists;
}