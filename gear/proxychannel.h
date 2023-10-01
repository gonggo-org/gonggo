#ifndef _GONGGO_PROXY_CHANNEL_H_
#define _GONGGO_PROXY_CHANNEL_H_

#include <stdbool.h>
#include <glib.h>
#include <civetweb.h>

#include "cJSON.h"

#include "define.h"
#include "log.h"

enum ChannelState {
    CHANNEL_IDLE = 0,
    CHANNEL_REQUEST = 1,
    CHANNEL_INVALID_TASK = 2,
    CHANNEL_INVALID_PAYLOAD = 3,
    CHANNEL_ACKNOWLEDGED = 4,
    CHANNEL_DONE = 5,
    CHANNEL_STOP = 6,
    CHANNEL_DEAD_REQUEST = 7
};

typedef struct ChannelSegmentData {
    pthread_mutex_t mtx;
    pthread_cond_t cond_idle;
    pthread_cond_t cond_dispatcher_wakeup;
    pthread_cond_t cond_proxy_wakeup;
    enum ChannelState state;
    char rid[UUIDBUFLEN]; //request id
    char task[TASKBUFLEN];
    unsigned int payload_buff_length;
} ChannelSegmentData;

extern char* proxy_channel_path(const char *sawang_name);
extern bool proxy_channel(const char *rid, const LogContext *log_ctx,
    struct mg_connection *conn,
    const char *sawang_name, const char *task, const cJSON *arg,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);
extern void proxy_channel_stop(const char *sawang_name, const LogContext *log_ctx);
extern void proxy_channel_dead_request(const char *sawang_name, GSList *rid_list,
    const LogContext *log_ctx, GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);

#endif //_GONGGO_PROXY_CHANNEL_H_
