#ifndef _GONGGO_PROXY_SUBSCRIBE_H_
#define _GONGGO_PROXY_SUBSCRIBE_H_

#include <stdbool.h>
#include <glib.h>

#include "define.h"
#include "log.h"
#include "db.h"

enum ProxySegment {
    CHANNEL,
    SUBSCRIBE,
    HEARTBEAT
};

enum SubscribeState {
    SUBSCRIBE_IDLE = 0,
    SUBSCRIBE_ANSWER = 1,
    SUBSCRIBE_FAILED = 2,
    SUBSCRIBE_DONE = 3
};

typedef struct ProxySubscribeSegmentData {
    pthread_mutex_t mtx;
    pthread_cond_t cond_dispatcher_wakeup;
    pthread_cond_t cond_proxy_wakeup;
    enum SubscribeState state;
    char aid[UUIDBUFLEN]; //answer id
    int payload_buff_length;
    bool remove_request;
} ProxySubscribeSegmentData;

typedef struct ProxySubscribeThreadData {
    const LogContext *log_ctx;
    GHashTable *request_table; //map rid to client conn
    pthread_mutex_t *mtx_request_table;
    char sawang_name[SHMPATHBUFLEN];
    ProxySubscribeSegmentData *segment;
    const DbContext *db_ctx;
    GHashTable *proxy_thread_table;
    pthread_mutex_t *mtx_thread_table;
    volatile bool stop;
    volatile bool die;
} ProxySubscribeThreadData;

extern const char* proxy_segment_label(enum ProxySegment segment);
extern ProxySubscribeThreadData* proxy_subscribe_thread_data_create(
    const LogContext *log_ctx, GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    const char *sawang_name, const DbContext *db_ctx,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);
extern void proxy_subscribe_thread_data_destroy(ProxySubscribeThreadData *data);
extern char* proxy_subscribe_path(const char *sawang_name);
extern void* proxy_subscribe_thread(void *arg);

#endif //_GONGGO_PROXY_SUBSCRIBE_H_
