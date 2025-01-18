#include <glib.h>

#include "proxysubscribethreadtable.h"

static pthread_mutex_t proxy_subscribe_thread_table_lock;
//map proxy-name to ProxySubscribeTableContext
static GHashTable *proxy_subscribe_thread_table = NULL;
static void proxy_subscribe_thread_table_key_destroy(gpointer data);
static void proxy_subscribe_thread_table_value_destroy(ProxySubscribeTableContext* p);

void proxy_subscribe_thread_table_create(void) {
    pthread_mutexattr_t mtx_attr;

    if(proxy_subscribe_thread_table==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&proxy_subscribe_thread_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        proxy_subscribe_thread_table = g_hash_table_new_full(g_str_hash, g_str_equal, proxy_subscribe_thread_table_key_destroy, 
            (GDestroyNotify)proxy_subscribe_thread_table_value_destroy);
    }
}

void proxy_subscribe_thread_table_destroy(void) {
    if(proxy_subscribe_thread_table!=NULL) {
        g_hash_table_destroy(proxy_subscribe_thread_table);
        proxy_subscribe_thread_table = NULL;
        pthread_mutex_destroy(&proxy_subscribe_thread_table_lock);
    }
}

void proxy_subscribe_thread_table_set(const char* proxy_name, pthread_t thread, ProxySubscribeContext *ctx) {
    ProxySubscribeTableContext* p;

    pthread_mutex_lock(&proxy_subscribe_thread_table_lock);
    p = (ProxySubscribeTableContext*)malloc(sizeof(ProxySubscribeTableContext));
    p->thread = thread;
    p->ctx = ctx;
    g_hash_table_remove(proxy_subscribe_thread_table, proxy_name);
    g_hash_table_insert(proxy_subscribe_thread_table, strdup(proxy_name), p); 
    pthread_mutex_unlock(&proxy_subscribe_thread_table_lock);
}

const ProxySubscribeTableContext* proxy_subscribe_thread_table_get(const char* proxy_name) {
    ProxySubscribeTableContext* p;

    pthread_mutex_lock(&proxy_subscribe_thread_table_lock);
    p = (ProxySubscribeTableContext*)g_hash_table_lookup(proxy_subscribe_thread_table, proxy_name);
    pthread_mutex_unlock(&proxy_subscribe_thread_table_lock);

    return p;
}

void proxy_subscribe_thread_table_remove(const char* proxy_name) {
    pthread_mutex_lock(&proxy_subscribe_thread_table_lock);
    g_hash_table_remove(proxy_subscribe_thread_table, proxy_name);
    pthread_mutex_unlock(&proxy_subscribe_thread_table_lock);
}

static void proxy_subscribe_thread_table_key_destroy(gpointer data) {
    if(data!=NULL) {
        free((char*)data);
    }
}

static void proxy_subscribe_thread_table_value_destroy(ProxySubscribeTableContext* p) {
    if(p!=NULL) {
        if(p->ctx!=NULL) {            
            proxy_subscribe_context_destroy(p->ctx);
        }        
        free(p);
    }
}