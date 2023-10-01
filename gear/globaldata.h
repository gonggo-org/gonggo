#ifndef _GONGGO_GLOBALDATA_H_
#define _GONGGO_GLOBALDATA_H_

#include <stdbool.h>
#include <glib.h>

#include "log.h"
#include "db.h"

typedef struct GlobalData {
    const LogContext *log_ctx;
    GHashTable *request_table; //map rid to client conn
    pthread_mutex_t *mtx_request_table;
    GHashTable *proxy_thread_table; //map proxy-name to thread id pointer
    pthread_mutex_t *mtx_thread_table;    
    const char *activation_path;
    const DbContext *db_ctx;
    int respond_query_size;
} GlobalData;

extern volatile bool gonggo_exit;
extern void global_data_setup(GlobalData *gd, const LogContext *log_ctx,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    const char *activation_path, const DbContext *db_ctx, int respond_query_size);

#endif //_GONGGO_GLOBALDATA_H_
