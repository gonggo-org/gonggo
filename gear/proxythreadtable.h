#ifndef _GONGGO_PROXY_THREAD_TABLE_H_
#define _GONGGO_PROXY_THREAD_TABLE_H_

#include <glib.h>

#include "proxysubscribe.h"
#include "proxyheartbeat.h"

typedef struct ProxyThreadTableValue {
    pthread_t subscribe_thread_id;
    ProxySubscribeThreadData *subscribe_thread_data;
    pthread_t heartbeat_thread_id;
    ProxyHeartbeatThreadData *heartbeat_thread_data;
} ProxyThreadTableValue;

extern GHashTable *proxy_thread_table_create();
extern void proxy_thread_table_destroy(GHashTable *proxy_thread_table);
extern void proxy_register(pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name,
    pthread_t subscribe_thread_id, ProxySubscribeThreadData *subscribe_thread_data,
    pthread_t heartbeat_thread_id, ProxyHeartbeatThreadData *heartbeat_thread_data);
extern bool proxy_remove_with_lock(const LogContext *log_ctx, pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name);
extern bool proxy_remove(const LogContext *log_ctx, GHashTable *proxy_thread_table, const char *sawang_name);
extern bool proxy_exists(pthread_mutex_t *lock, GHashTable *proxy_thread_table, const char *sawang_name);
extern void proxy_thread_stop(pthread_t subscribe_thread_id, ProxySubscribeThreadData *subscribe_thread_data,
    pthread_t heartbeat_thread_id, ProxyHeartbeatThreadData *heartbeat_thread_data,
    bool destroy);
extern void proxy_thread_stop_async(const LogContext *log_ctx,
    pthread_mutex_t *mtx_thread_table, GHashTable *proxy_thread_table, const char* sawang_name);

#endif //_GONGGO_PROXY_THREAD_TABLE_H_
