#ifndef _GONGGO_PROXY_HEARTBEAT_H_
#define _GONGGO_PROXY_HEARTBEAT_H_

#include <stdbool.h>
#include <glib.h>

#include "define.h"
#include "log.h"

typedef struct ProxyHeartbeatSegmentData {
    pthread_mutex_t mtx;
    pthread_cond_t wakeup;
    time_t overdue;
} ProxyHeartbeatSegmentData;

typedef struct ProxyHeartbeatThreadData {
    long heartbeat_wait;
    const LogContext *log_ctx;
    char sawang_name[SHMPATHBUFLEN];
    ProxyHeartbeatSegmentData *segment;
    pthread_mutex_t mtx;
    pthread_cond_t wakeup;
    GHashTable *proxy_thread_table;
    volatile bool stop;
    volatile bool die;
    pthread_mutex_t *mtx_thread_table;
} ProxyHeartbeatThreadData;

extern ProxyHeartbeatThreadData* proxy_heartbeat_thread_data_create(const LogContext *log_ctx, const char *sawang_name,
    long heartbeat_wait, GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);
extern void proxy_heartbeat_thread_data_destroy(ProxyHeartbeatThreadData *data);
extern char* proxy_heartbeat_path(const char *sawang_name);
extern void* proxy_heartbeat_thread(void *arg);
extern bool proxy_heart_beat_overdue(ProxyHeartbeatThreadData *data);

#endif //_GONGGO_PROXY_HEARTBEAT_H_
