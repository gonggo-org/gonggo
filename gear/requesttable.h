#ifndef _GONGGO_REQUEST_TABLE_H_
#define _GONGGO_REQUEST_TABLE_H_

#include <glib.h>
#include <stdbool.h>
#include <civetweb.h>

#include "log.h"

extern GHashTable *request_table_create();
extern void request_table_destroy(GHashTable *request_table);
extern void request_table_entry(GHashTable *request_table, pthread_mutex_t *lock,
    const char *rid, struct mg_connection *conn, const char *sawang_name);
extern void request_table_conn_remove(
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table, 
    const struct mg_connection *conn, const LogContext *log_ctx);
extern struct mg_connection* request_table_conn_get(GHashTable *request_table, pthread_mutex_t *lock,
    const char *rid, bool update_timestamp, bool remove_request);

#endif //_GONGGO_REQUEST_TABLE_H_
