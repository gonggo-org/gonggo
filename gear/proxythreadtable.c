#include "proxythreadtable.h"

typedef struct ProxyStopAsyncThreadData {
    const LogContext *log_ctx;
    pthread_mutex_t *mtx_thread_table;
    GHashTable *proxy_thread_table;
    char sawang_name[SHMPATHBUFLEN];
} ProxyStopAsyncThreadData;

static void *proxy_stop_async(void *arg);

static void proxy_thread_table_key_destroy(gpointer data);
static void proxy_thread_table_value_destroy(gpointer data);

GHashTable *proxy_thread_table_create() {
    return g_hash_table_new_full( g_str_hash, g_str_equal, proxy_thread_table_key_destroy,
        proxy_thread_table_value_destroy );
}

void proxy_thread_table_destroy(GHashTable *proxy_thread_table) {
    g_hash_table_destroy(proxy_thread_table);
}

void proxy_register(pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name,
    pthread_t subscribe_thread_id, ProxySubscribeThreadData *subscribe_thread_data,
    pthread_t heartbeat_thread_id, ProxyHeartbeatThreadData *heartbeat_thread_data)
{
    ProxyThreadTableValue*  value;

    pthread_mutex_lock(lock);
    value = (ProxyThreadTableValue*)malloc(sizeof(ProxyThreadTableValue));
    value->subscribe_thread_id = subscribe_thread_id;
    value->subscribe_thread_data = subscribe_thread_data;
    value->heartbeat_thread_id = heartbeat_thread_id;
    value->heartbeat_thread_data = heartbeat_thread_data;
    g_hash_table_insert(proxy_thread_table, strdup(sawang_name), value);
    pthread_mutex_unlock(lock);
}

bool proxy_remove_with_lock(const LogContext *log_ctx, pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name) {
    bool status;

    pthread_mutex_lock(lock);
    status = proxy_remove(log_ctx, proxy_thread_table, sawang_name);
    pthread_mutex_unlock(lock);

    return status;
}

bool proxy_remove(const LogContext *log_ctx, GHashTable *proxy_thread_table, const char *sawang_name) {
    gpointer pointer;
    ProxyThreadTableValue *proxy_thread_table_value;

    pointer = g_hash_table_lookup(proxy_thread_table, sawang_name);
    if(pointer==NULL)
        return false;

    proxy_thread_table_value = (ProxyThreadTableValue*)pointer;

    gonggo_log(log_ctx, "INFO", "subscibe and heartbeat threads for proxy %s stopping", sawang_name);
    proxy_thread_stop(
        proxy_thread_table_value->subscribe_thread_id, proxy_thread_table_value->subscribe_thread_data,
        proxy_thread_table_value->heartbeat_thread_id, proxy_thread_table_value->heartbeat_thread_data,
        false/*let g_hash_table_remove destroy*/);
    gonggo_log(log_ctx, "INFO", "subscibe and heartbeat threads for proxy %s stopped", sawang_name);
    g_hash_table_remove(proxy_thread_table, sawang_name);
    gonggo_log(log_ctx, "INFO", "unregister proxy %s done", sawang_name);

    return true;
}

bool proxy_exists(pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name) {
    bool exists;

    pthread_mutex_lock(lock);
    exists = g_hash_table_contains(proxy_thread_table, sawang_name);
    pthread_mutex_unlock(lock);
    return exists;
}

void proxy_thread_stop(pthread_t subscribe_thread_id, ProxySubscribeThreadData *subscribe_thread_data,
    pthread_t heartbeat_thread_id, ProxyHeartbeatThreadData *heartbeat_thread_data,
    bool destroy)
{
    if( subscribe_thread_data!=NULL && !subscribe_thread_data->die ) {
        subscribe_thread_data->stop = true;
        pthread_mutex_lock(&subscribe_thread_data->segment->mtx);
        if( subscribe_thread_data->segment->state != SUBSCRIBE_ANSWER )
            pthread_cond_signal(&subscribe_thread_data->segment->cond_dispatcher_wakeup);
        pthread_mutex_unlock(&subscribe_thread_data->segment->mtx);
    }

    if( heartbeat_thread_data!=NULL && !heartbeat_thread_data->die ) {
        heartbeat_thread_data->stop = true;
        pthread_mutex_lock( &heartbeat_thread_data->mtx );
        pthread_cond_signal(&heartbeat_thread_data->wakeup);
        pthread_mutex_unlock( &heartbeat_thread_data->mtx );
    }

    if( subscribe_thread_data!=NULL && !subscribe_thread_data->die )
        pthread_join(subscribe_thread_id, NULL);

    if( heartbeat_thread_data!=NULL && !heartbeat_thread_data->die )
        pthread_join(heartbeat_thread_id, NULL);

    if( destroy ) {
        if( subscribe_thread_data!=NULL ) {
            proxy_subscribe_thread_data_destroy(subscribe_thread_data);
            free(subscribe_thread_data);
        }

        if( heartbeat_thread_data!=NULL ) {
            proxy_heartbeat_thread_data_destroy(heartbeat_thread_data);
            free(heartbeat_thread_data);
        }
    }
}

void proxy_thread_stop_async(const LogContext *log_ctx,
    pthread_mutex_t *mtx_thread_table, GHashTable *proxy_thread_table, const char* sawang_name)
{
    ProxyStopAsyncThreadData *td;
    pthread_attr_t attr;
    pthread_t t_proxy_stop;

    td = (ProxyStopAsyncThreadData*)malloc(sizeof(ProxyStopAsyncThreadData));
    td->log_ctx = log_ctx;
    td->mtx_thread_table = mtx_thread_table;
    td->proxy_thread_table = proxy_thread_table;
    strcpy(td->sawang_name, sawang_name);
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create( &t_proxy_stop, &attr, proxy_stop_async, td );
    pthread_attr_destroy(&attr);
}

static void proxy_thread_table_key_destroy(gpointer data) {
    if(data!=NULL)
        free((char*)data);
    return;
}

static void proxy_thread_table_value_destroy(gpointer data) {
    if(data!=NULL) {
        ProxyThreadTableValue* p = (ProxyThreadTableValue*)data;
        if(p->subscribe_thread_data!=NULL) {
            proxy_subscribe_thread_data_destroy(p->subscribe_thread_data);
            free(p->subscribe_thread_data);
            p->subscribe_thread_data = NULL;
        }
        if(p->heartbeat_thread_data!=NULL) {
            proxy_heartbeat_thread_data_destroy(p->heartbeat_thread_data);
            free(p->heartbeat_thread_data);
            p->heartbeat_thread_data = NULL;
        }
        free(p);
    }
    return;
}

static void *proxy_stop_async(void *arg) {
    ProxyStopAsyncThreadData *td;

    td = (ProxyStopAsyncThreadData*)arg;
    proxy_remove_with_lock(td->log_ctx, td->mtx_thread_table, td->proxy_thread_table, td->sawang_name);
    free(td);
    pthread_exit(NULL);
}
