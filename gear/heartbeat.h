#ifndef _GONGGO_HEARTBEAT_H_
#define _GONGGO_HEARTBEAT_H_

#include "log.h"

typedef struct HeartbeatData {
    pthread_mutex_t mtx;
    pthread_cond_t wakeup;
    time_t overdue;
} HeartbeatData;

typedef struct HeartbeatThreadData {
    const LogContext *log_ctx;
    long heartbeat_period;
    float heartbeat_timeout;
    const char *gonggo_heartbeat_path;
    HeartbeatData *segment;
    volatile bool started;
} HeartbeatThreadData;

extern void heartbeat_thread_data_init(HeartbeatThreadData *data);
extern void heartbeat_thread_data_setup(HeartbeatThreadData *data, const LogContext *log_ctx,
    long heartbeat_period, float heartbeat_timeout, const char *gonggo_heartbeat_path);
extern void heartbeat_thread_data_destroy(HeartbeatThreadData *data);
extern void heartbeat_thread_stop(HeartbeatThreadData *data, pthread_t thread_id);
extern void* heartbeat(void *arg);

#endif //_GONGGO_HEARTBEAT_H_
