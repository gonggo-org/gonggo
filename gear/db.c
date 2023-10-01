#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sqlite3.h>
#include "db.h"

static char *db_path(const char *path, const char *gonggo_name);
static void db_close(sqlite3 *p_db);
static bool db_error(int status, const LogContext *log_ctx, const char *info, const char *msg);

#define TMSTRBUFLEN 35

void db_context_setup(DbContext *ctx, const char *gonggo_name, const char *path, pthread_mutex_t *lock) {
    ctx->gonggo_name = gonggo_name;
    ctx->path = path;
    ctx->lock = lock;
}

bool db_ensure(const DbContext *ctx, const LogContext *log_ctx) {
    sqlite3 *p_db;
    int status;
    char *filepath;
    const char *sql;
    char *errmsg;
    bool result;

    p_db = NULL;
    errmsg = NULL;

    result = false;
    do {
        filepath = db_path(ctx->path, ctx->gonggo_name);
        status = sqlite3_open(filepath, &p_db);
        free(filepath);
        if( db_error(status, log_ctx, "db open error", NULL) )
            break;

        sql = "CREATE TABLE IF NOT EXISTS request ("
            "id TEXT NOT NULL,"
            "num INTEGER NOT NULL,"
            "respond TEXT NOT NULL,"
            "ts DATETIME NOT NULL DEFAULT (datetime(CURRENT_TIMESTAMP, 'localtime')),"
            "PRIMARY KEY (id, num)"
            ");";
        status = sqlite3_exec(p_db, sql, NULL, NULL, &errmsg);
        if( db_error(status, log_ctx, "table request create", errmsg) )
            break;

        result = true;
    } while(false);

    if(errmsg!=NULL)
        sqlite3_free(errmsg);
    if(p_db!=NULL)
        db_close(p_db);
    return result;
}

bool db_respond_insert(const DbContext *ctx, const LogContext *log_ctx, const char *rid, const char *respond) {
    char *filepath;
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status, num;
    bool result;

    p_db = NULL;
    p_stmt = NULL;
    num = 0;

    pthread_mutex_lock(ctx->lock);
    result = false;

    do {
        filepath = db_path(ctx->path, ctx->gonggo_name);
        status = sqlite3_open(filepath, &p_db);
        free(filepath);
        if( db_error(status, log_ctx, "db open for request insert", NULL) )
            break;

        status = sqlite3_prepare_v2(p_db, "SELECT MAX(num) FROM request WHERE id = ?", -1, &p_stmt, NULL);
        if( db_error(status, log_ctx, "prepare statement for max num query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding request-id for max num query", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status == SQLITE_ROW )
            num = sqlite3_column_int( p_stmt, 0 );
        else if( status != SQLITE_DONE ) {
            db_error(status, log_ctx, "max num query", NULL);
            break;
        }
        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        status = sqlite3_prepare_v2(p_db, "INSERT INTO request (id, num, respond) VALUES (?, ?, ?)", -1, &p_stmt, NULL);
        if( db_error(status, log_ctx, "prepare statement for request insert", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding request-id for request insert", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, num + 1 );
        if( db_error(status, log_ctx, "binding num for request insert", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 3, respond, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding respond for request insert", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status != SQLITE_DONE ) {
            db_error(status, log_ctx, "inserting respond into request table", NULL);
            break;
        }

        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        result = true;
    } while(false);

    if(p_stmt!=NULL)
        sqlite3_finalize(p_stmt);
    if(p_db!=NULL)
        db_close(p_db);

    pthread_mutex_unlock(ctx->lock);
    return result;
}

bool db_respond_retain(const DbContext *ctx, const LogContext *log_ctx, long overdue) {
    char *filepath;
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status;
    bool result;
    char tmstr[TMSTRBUFLEN];
    time_t t;
    struct tm tm_now;

    p_db = NULL;
    p_stmt = NULL;

    pthread_mutex_lock(ctx->lock);

    result = false;
    do {
        filepath = db_path(ctx->path, ctx->gonggo_name);
        status = sqlite3_open(filepath, &p_db);
        free(filepath);
        if( db_error(status, log_ctx, "db open for overdue respond deletion", NULL) )
            break;

        t = time(NULL) - overdue;
        tm_now = *localtime(&t);
        strftime(tmstr, TMSTRBUFLEN, "%Y-%m-%d %H:%M:%S %Z", &tm_now);

        status = sqlite3_prepare_v2(p_db, "DELETE FROM request WHERE ts < ?", -1, &p_stmt, NULL);
        if( db_error(status, log_ctx, "prepare statement for overdue respond deletion", NULL) )
            break;

        status = sqlite3_bind_text( p_stmt, 1, tmstr, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding time for overdue respond deletion", NULL) )
            break;

        status = sqlite3_step( p_stmt );
        if( status != SQLITE_DONE ) {
            db_error(status, log_ctx, "overdue respond deletion", NULL);
            break;
        }

        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        result = true;
    } while(false);

    if(p_stmt!=NULL)
        sqlite3_finalize(p_stmt);
    if(p_db!=NULL)
        db_close(p_db);

    pthread_mutex_unlock(ctx->lock);

    return result;
}

void db_respond_query_result_init(RespondQueryResult *r) {
    r->respond = NULL;
    r->more = 0;
    r->stop = 0;
}

void db_respond_query_result_destroy(RespondQueryResult *r) {
    char **run;
    if(r->respond!=NULL) {
        run = r->respond;
        while( *run != NULL ) {
            free(*run);
            run++;
        }
        free(r->respond);
    }
}

bool db_respond_query(const DbContext *ctx, const LogContext *log_ctx, const char *rid, int start, int size, RespondQueryResult *r) {
    char *filepath;
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status, total, count;
    char **run;
    bool result;

    p_db = NULL;
    p_stmt = NULL;
    total = 0;

    db_respond_query_result_init(r);
    pthread_mutex_lock(ctx->lock);

    result = false;
    do {
        filepath = db_path(ctx->path, ctx->gonggo_name);
        status = sqlite3_open_v2(filepath, &p_db, SQLITE_OPEN_READONLY, NULL);
        free(filepath);
        if( db_error(status, log_ctx, "db open for overdue respond deletion", NULL) )
            break;
        status = sqlite3_prepare_v2(p_db, "SELECT COUNT(*) FROM request WHERE id = ? AND num>= ?", -1, &p_stmt, NULL);
        if( db_error(status, log_ctx, "prepare statement for respond count query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding request-id for respond count query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, start );
        if( db_error(status, log_ctx, "binding start for respond count query", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status == SQLITE_ROW )
            total = sqlite3_column_int( p_stmt, 0 );
        else if( status != SQLITE_DONE ) {
            db_error(status, log_ctx, "respond count query", NULL);
            break;
        }
        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        if( total<1 ) {
            result = true;
            break;
        }

        r->more = total > size ? total - size : 0;
        count = total > size ? size : total;
        r->respond = (char**)malloc((count + 1/*NULL terminator*/) * sizeof(char*));
        status = sqlite3_prepare_v2(p_db, "SELECT num, respond FROM request WHERE id = ? AND num>= ? ORDER BY num LIMIT ?", -1, &p_stmt, NULL);
        if( db_error(status, log_ctx, "prepare statement for respond query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, log_ctx, "binding request-id for respond query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, start );
        if( db_error(status, log_ctx, "binding start for respond query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 3, count );
        if( db_error(status, log_ctx, "binding limit for respond query", NULL) )
            break;
        result = true;
        run = r->respond;
        while( (status = sqlite3_step( p_stmt )) == SQLITE_ROW ) {
            r->stop = sqlite3_column_int( p_stmt, 0 );
            *(run++) = strdup( (char*)sqlite3_column_text(p_stmt, 1) );
        }
        *run = NULL;//array terminator
        if( status!=SQLITE_DONE ) {
            db_error(status, log_ctx, "respond query", NULL);
            result = false;
            break;
        }

        sqlite3_finalize(p_stmt);
        p_stmt = NULL;
    } while(false);

    if(p_stmt!=NULL)
        sqlite3_finalize(p_stmt);
    if(p_db!=NULL)
        db_close(p_db);

    pthread_mutex_unlock(ctx->lock);

    return result;
}

static char *db_path(const char *path, const char *gonggo_name) {
    char *p;

    p = (char*)malloc(strlen(path) + strlen(gonggo_name) + 5);
    sprintf(p, "%s/%s.db", path, gonggo_name);
    return p;
}

static void db_close(sqlite3 *p_db) {
    if(p_db!=NULL)
        sqlite3_close(p_db);
}

static bool db_error(int status, const LogContext *log_ctx, const char *info, const char *msg) {
    if(status != SQLITE_OK) {
        if(msg!=NULL)
            gonggo_log(log_ctx, "ERROR", "%s is failed with status %d %s (%s)", info, status, sqlite3_errstr(status), msg);
        else
            gonggo_log(log_ctx, "ERROR", "%s is failed with status %d %s", info, status, sqlite3_errstr(status));
    }
    return status != SQLITE_OK;
}
