#include <glib.h>

#include "proxyalive.h"
#include "proxyalivethreadtable.h"

static pthread_mutex_t proxy_alive_thread_table_lock;
//map proxy-name to ProxyAliveTableContext
static GHashTable *proxy_alive_thread_table = NULL;
static void proxy_alive_thread_table_key_destroy(gpointer data);
static void proxy_alive_thread_table_value_destroy(ProxyAliveTableContext* p);

void proxy_alive_thread_table_create(void) {
    pthread_mutexattr_t mtx_attr;

    if(proxy_alive_thread_table==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&proxy_alive_thread_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        proxy_alive_thread_table = g_hash_table_new_full(g_str_hash, g_str_equal, proxy_alive_thread_table_key_destroy, 
            (GDestroyNotify)proxy_alive_thread_table_value_destroy);
    }
}

void proxy_alive_thread_table_destroy(void) {
    if(proxy_alive_thread_table!=NULL) {
        g_hash_table_destroy(proxy_alive_thread_table);
        proxy_alive_thread_table = NULL;
        pthread_mutex_destroy(&proxy_alive_thread_table_lock);
    }
}

void proxy_alive_thread_table_set(const char* proxy_name, pthread_t thread, ProxyAliveContext *ctx) {
    ProxyAliveTableContext* p;

    pthread_mutex_lock(&proxy_alive_thread_table_lock);
    p = (ProxyAliveTableContext*)malloc(sizeof(ProxyAliveTableContext));
    p->thread = thread;
    p->ctx = ctx;
    g_hash_table_remove(proxy_alive_thread_table, proxy_name);
    g_hash_table_insert(proxy_alive_thread_table, strdup(proxy_name), p); 
    pthread_mutex_unlock(&proxy_alive_thread_table_lock);    
}

const ProxyAliveTableContext* proxy_alive_thread_table_get(const char* proxy_name) {
    ProxyAliveTableContext* p;

    pthread_mutex_lock(&proxy_alive_thread_table_lock);
    p = (ProxyAliveTableContext*)g_hash_table_lookup(proxy_alive_thread_table, proxy_name);
    pthread_mutex_unlock(&proxy_alive_thread_table_lock);

    return p;
}

void proxy_alive_thread_table_remove(const char* proxy_name) {
    pthread_mutex_lock(&proxy_alive_thread_table_lock);
    g_hash_table_remove(proxy_alive_thread_table, proxy_name);
    pthread_mutex_unlock(&proxy_alive_thread_table_lock);
}

void proxy_alive_thread_table_char_keys_create(StrArr *sa) {
    pthread_mutex_lock(&proxy_alive_thread_table_lock);
    str_arr_create_keys(proxy_alive_thread_table, sa);
    pthread_mutex_unlock(&proxy_alive_thread_table_lock);
}

static void proxy_alive_thread_table_key_destroy(gpointer data) {
    if(data!=NULL) {
        free((char*)data);
    }
    return;
}

static void proxy_alive_thread_table_value_destroy(ProxyAliveTableContext* p) {
    if(p!=NULL) {
        if(p->ctx!=NULL) {
            proxy_alive_context_destroy(p->ctx);
        }
        free(p);
    }
}