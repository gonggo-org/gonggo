#ifndef _GONGGO_PROXY_ACTIVATOR_H_
#define _GONGGO_PROXY_ACTIVATOR_H_

#include <civetweb.h>

#include "proxyheartbeat.h"
#include "db.h"

enum ActivationState {
  ACTIVATION_IDLE = 0,

  ACTIVATION_REQUEST = 1,
  ACTIVATION_FAILED = 2,
  ACTIVATION_SUCCESS = 3,
  ACTIVATION_DONE = 4,

  DEACTIVATION_REQUEST = 11,
  DEACTIVATION_NOTEXISTS = 12,
  DEACTIVATION_SUCCESS = 13,
  DEACTIVATION_DONE = 14
};

typedef struct ActivationData {
    pthread_mutex_t mtx;
    pthread_cond_t cond_idle;
    pthread_cond_t cond_dispatcher_wakeup;
    pthread_cond_t cond_proxy_wakeup;
    char sawang_name[SHMPATHBUFLEN];
    long proxy_heartbeat;
    long dispatcher_heartbeat;
    enum ActivationState state;
} ActivationData;

typedef struct ProxyActivationThreadData {
    const LogContext *log_ctx;
    const char *gonggo_path;
    GHashTable *request_table; //map rid to client conn
    pthread_mutex_t *mtx_request_table;
    GHashTable *proxy_thread_table; //map proxy-name to thread id pointer
    pthread_mutex_t *mtx_thread_table;
    long heartbeat_period;
    ActivationData *segment;
    const DbContext *db_ctx;
    volatile bool started;
} ProxyActivationThreadData;

extern void proxy_activation_thread_data_init(ProxyActivationThreadData *data);
extern void proxy_activation_thread_data_setup(ProxyActivationThreadData *data,
    const LogContext *log_ctx, const char *gonggo_path,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    long heartbeat_period,const DbContext *db_ctx);
extern void proxy_activation_thread_stop(ProxyActivationThreadData *data, pthread_t thread_id);
extern void proxy_activation_thread_data_destroy(ProxyActivationThreadData *data);
extern void* proxy_activation_listener(void *arg);

#endif //_GONGGO_PROXY_ACTIVATOR_H_
