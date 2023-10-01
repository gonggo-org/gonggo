#ifndef _GONGGO_DB_H_
#define _GONGGO_DB_H_

#include "log.h"

typedef struct DbContext {
    const char *gonggo_name;
    const char *path;
    pthread_mutex_t *lock;
} DbContext;

typedef struct RespondQueryResult {
    char **respond;//respond array terminated by NULL
    int stop;
    int more;
} RespondQueryResult;

extern void db_context_setup(DbContext *ctx, const char *gonggo_name, const char *path, pthread_mutex_t *lock);
extern bool db_ensure(const DbContext *ctx, const LogContext *log_ctx);
extern bool db_respond_insert(const DbContext *ctx, const LogContext *log_ctx, const char *rid, const char *respond);
extern bool db_respond_retain(const DbContext *ctx, const LogContext *log_ctx, long overdue);
extern void db_respond_query_result_init(RespondQueryResult *r);
extern void db_respond_query_result_destroy(RespondQueryResult *r);
extern bool db_respond_query(const DbContext *ctx, const LogContext *log_ctx, const char *rid, int start, int size, RespondQueryResult *r);

#endif //_GONGGO_DB_H_
