#include "log.h"
#include "proxysubscribethreadtable.h"
#include "proxychannelthreadtable.h"
#include "proxyalivethreadtable.h"
#include "proxyalive.h"
#include "strarr.h"
#include "util.h"

static bool has_proxy_thread_kill_lock = false;
static pthread_mutex_t proxy_thread_kill_lock;

static bool has_proxy_zombie_lock = false;
static pthread_mutex_t proxy_zombie_lock;
static GPtrArray* proxy_terminating_arr = NULL;

void proxy_thread_killer_init(void) {
    if(!has_proxy_thread_kill_lock || !has_proxy_zombie_lock) {
        pthread_mutexattr_t mtx_attr;
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        if(!has_proxy_thread_kill_lock) {
            pthread_mutex_init(&proxy_thread_kill_lock, &mtx_attr);
            has_proxy_thread_kill_lock = true;
        }
        if(!has_proxy_zombie_lock) {
            pthread_mutex_init(&proxy_zombie_lock, &mtx_attr);
            has_proxy_zombie_lock = true;
        }
        pthread_mutexattr_destroy(&mtx_attr);        
    }

    if(proxy_terminating_arr==NULL) {
        proxy_terminating_arr = g_ptr_array_new_full(1, (GDestroyNotify)free);
    }
}

void proxy_thread_terminating_set(const char *proxy_name) {
    pthread_mutex_lock(&proxy_zombie_lock);
    if(!g_ptr_array_find_with_equal_func(proxy_terminating_arr, proxy_name, (GEqualFunc)str_equal, NULL)){
        g_ptr_array_add(proxy_terminating_arr, strdup(proxy_name));
    }
    pthread_mutex_unlock(&proxy_zombie_lock);    
}

void proxy_thread_terminating_drop(const char *proxy_name) {
    guint idx;
    pthread_mutex_lock(&proxy_zombie_lock);
    if(g_ptr_array_find_with_equal_func(proxy_terminating_arr, proxy_name, (GEqualFunc)str_equal, &idx)){
        g_ptr_array_remove_index(proxy_terminating_arr, idx);
    }
    pthread_mutex_unlock(&proxy_zombie_lock);    
}


bool proxy_thread_zombie(const char *proxy_name) {
    bool exists;
    pthread_mutex_lock(&proxy_zombie_lock);
    exists = g_ptr_array_find_with_equal_func(proxy_terminating_arr, proxy_name, (GEqualFunc)str_equal, NULL);
    pthread_mutex_unlock(&proxy_zombie_lock);
    return exists;
}

void proxy_thread_kill(const char* proxy_name) 
{
    const ProxySubscribeTableContext* p_subscribe;
    const ProxyChannelTableContext* p_channel;
    const ProxyAliveTableContext* p_alive;

    pthread_mutex_lock(&proxy_thread_kill_lock);

    p_subscribe = proxy_subscribe_thread_table_get(proxy_name);
    if(p_subscribe!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s proxy_subscribe_stop starts", proxy_name);
        proxy_subscribe_stop(p_subscribe->ctx);
        gonggo_log("INFO", "proxy_thread_kill %s proxy_subscribe_stop done", proxy_name);
    }

    proxy_channel_thread_table_lock_hold(true);
    p_channel = proxy_channel_thread_table_get(proxy_name, false);
    if(p_channel!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s proxy_channel_stop starts", proxy_name);
        proxy_channel_stop(p_channel->ctx);
        gonggo_log("INFO", "proxy_thread_kill %s proxy_channel_stop done", proxy_name);
    }
    proxy_channel_thread_table_lock_hold(false);

    p_alive = proxy_alive_thread_table_get(proxy_name);
    if(p_alive!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s proxy_alive_stop starts", proxy_name);
        proxy_alive_stop(p_alive->ctx);
        gonggo_log("INFO", "proxy_thread_kill %s proxy_alive_stop done", proxy_name);
    }

    if(p_subscribe!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s subscribe joining", proxy_name);
        pthread_join(p_subscribe->thread, NULL);
        gonggo_log("INFO", "proxy_thread_kill %s subscribe joining done", proxy_name);
        //remove from table and destroy context
        proxy_subscribe_thread_table_remove(proxy_name);
    }

    if(p_channel!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s channel joining", proxy_name);
        pthread_join(p_channel->thread, NULL);
        gonggo_log("INFO", "proxy_thread_kill %s channel joining done", proxy_name);
        //remove from table and destroy context
        proxy_channel_thread_table_remove(proxy_name);
    }

    if(p_alive!=NULL) {
        gonggo_log("INFO", "proxy_thread_kill %s alive joining", proxy_name);
        pthread_join(p_alive->thread, NULL);
        gonggo_log("INFO", "proxy_thread_kill %s alive joining done", proxy_name);
        //remove from table and destroy context
        proxy_alive_thread_table_remove(proxy_name);        
    }

    pthread_mutex_unlock(&proxy_thread_kill_lock);    
    gonggo_log("INFO", "proxy_thread_kill %s exit", proxy_name);
}

void proxy_thread_kill_all(void)
{
    StrArr sa;
    unsigned int i;
    char *proxy_name;

    gonggo_log("INFO", "proxy_thread_kill_all enter");
    proxy_alive_thread_table_char_keys_create(&sa);
    for(i=0; i<sa.len; i++) {
        proxy_name = sa.arr[i];
        gonggo_log("INFO", "terminate thread proxy %s starts", proxy_name);        
        proxy_thread_terminating_set(proxy_name);
        proxy_thread_kill(proxy_name);
        proxy_thread_terminating_drop(proxy_name);        
        gonggo_log("INFO", "terminate thread proxy %s done", proxy_name);
        free(proxy_name);
    }
    str_arr_destroy(&sa, false);
    gonggo_log("INFO", "proxy_thread_kill_all exit");
}


void proxy_thread_killer_destroy(void) {
    if(has_proxy_thread_kill_lock) {
        pthread_mutex_destroy(&proxy_thread_kill_lock);
        has_proxy_thread_kill_lock = false;
    }
    if(has_proxy_zombie_lock) {
        pthread_mutex_destroy(&proxy_zombie_lock);
        has_proxy_zombie_lock = false;
    }
    if(proxy_terminating_arr!=NULL) {
        g_ptr_array_free(proxy_terminating_arr, true);
        proxy_terminating_arr = NULL;
    }
}