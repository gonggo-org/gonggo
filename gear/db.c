#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sqlite3.h>

#include "log.h"
#include "db.h"

#define TMSTRBUFLEN 35

static char *db_path = NULL;

static bool has_db_lock = false;
static pthread_mutex_t db_lock;

static bool db_error(int status, const char *info, const char *msg);
static void db_close(sqlite3 *p_db);

void db_context_init(const char *gonggo_name, const char *path) {
    if(!has_db_lock) {
        pthread_mutexattr_t mtx_attr;
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&db_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);
        has_db_lock = true;
    }
    if(db_path==NULL) {
        db_path = (char*)malloc(strlen(path) + strlen(gonggo_name) + 5);
        sprintf(db_path, "%s/%s.db", path, gonggo_name);        
    }
}

void db_context_destroy(void) {
    if(has_db_lock) {
        pthread_mutex_destroy(&db_lock);
        has_db_lock = false;
    }
    if(db_path!=NULL) {
        free(db_path);
        db_path = NULL;
    }
}

bool db_ensure(void) {
    sqlite3 *p_db = NULL;
    bool result = false;
    int status;
    const char *sql;
    char *errmsg = NULL;

    do {
        status = sqlite3_open(db_path, &p_db);
        if( db_error(status, "db open error", NULL) ) {
            break;
        }

        sql = "CREATE TABLE IF NOT EXISTS request ("
            "id TEXT NOT NULL,"
            "num INTEGER NOT NULL,"
            "respond TEXT NOT NULL,"
            "ts DATETIME NOT NULL DEFAULT (datetime(CURRENT_TIMESTAMP, 'localtime')),"
            "PRIMARY KEY (id, num)"
            ");";
        status = sqlite3_exec(p_db, sql, NULL, NULL, &errmsg);
        if( db_error(status, "table request create", errmsg) )
            break;

        result = true;
    } while(false);

    if(errmsg!=NULL) {
        sqlite3_free(errmsg);
    }

    if(p_db!=NULL) {
        db_close(p_db);
    }

    return result;
}

bool db_respond_purge(long overdue) {
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status;
    bool result;
    char tmstr[TMSTRBUFLEN];
    time_t t;
    struct tm tm_now;

    p_db = NULL;
    p_stmt = NULL;

    pthread_mutex_lock(&db_lock);

    result = false;
    do {
        status = sqlite3_open(db_path, &p_db);
        if( db_error(status, "db open for overdue respond deletion", NULL) )
            break;

        t = time(NULL) - overdue;
        tm_now = *localtime(&t);
        strftime(tmstr, TMSTRBUFLEN, "%Y-%m-%d %H:%M:%S %Z", &tm_now);

        status = sqlite3_prepare_v2(p_db, "DELETE FROM request WHERE ts < ?", -1, &p_stmt, NULL);
        if(db_error(status, "prepare statement for overdue respond deletion", NULL)) {
            break;
        }

        status = sqlite3_bind_text( p_stmt, 1, tmstr, -1,  SQLITE_STATIC);
        if(db_error(status, "binding time for overdue respond deletion", NULL)) {
            break;
        }

        status = sqlite3_step( p_stmt );
        if(status != SQLITE_DONE) {
            db_error(status, "overdue respond deletion", NULL);
            break;
        }

        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        result = true;
    } while(false);

    if(p_stmt!=NULL) {
        sqlite3_finalize(p_stmt);
    }

    if(p_db!=NULL) {
        db_close(p_db);
    }

    pthread_mutex_unlock(&db_lock);

    return result;    
}

bool db_respond_insert(const char *rid, const char *respond) {
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status, num;
    bool result;

    p_db = NULL;
    p_stmt = NULL;
    num = 0;

    pthread_mutex_lock(&db_lock);
    result = false;

    do {
        status = sqlite3_open(db_path, &p_db);
        if( db_error(status, "db open for request insert", NULL) )
            break;

        status = sqlite3_prepare_v2(p_db, "SELECT MAX(num) FROM request WHERE id = ?", -1, &p_stmt, NULL);
        if( db_error(status, "prepare statement for max num query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, "binding request-id for max num query", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status == SQLITE_ROW )
            num = sqlite3_column_int( p_stmt, 0 );
        else if( status != SQLITE_DONE ) {
            db_error(status, "max num query", NULL);
            break;
        }
        sqlite3_finalize(p_stmt);
        p_stmt = NULL;

        status = sqlite3_prepare_v2(p_db, "INSERT INTO request (id, num, respond) VALUES (?, ?, ?)", -1, &p_stmt, NULL);
        if( db_error(status, "prepare statement for request insert", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, "binding request-id for request insert", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, num + 1 );
        if( db_error(status, "binding num for request insert", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 3, respond, -1,  SQLITE_STATIC);
        if( db_error(status, "binding respond for request insert", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status != SQLITE_DONE ) {
            db_error(status, "inserting respond into request table", NULL);
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

    pthread_mutex_unlock(&db_lock);
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

bool db_respond_query(const char *rid, int start, int size, RespondQueryResult *r) {
    sqlite3 *p_db;
    sqlite3_stmt *p_stmt;
    int status, total, count;
    char **run;
    bool result;

    p_db = NULL;
    p_stmt = NULL;
    total = 0;

    db_respond_query_result_init(r);
    pthread_mutex_lock(&db_lock);

    result = false;
    do {
        status = sqlite3_open_v2(db_path, &p_db, SQLITE_OPEN_READONLY, NULL);
        if( db_error(status, "db open for overdue respond deletion", NULL) )
            break;
        status = sqlite3_prepare_v2(p_db, "SELECT COUNT(*) FROM request WHERE id = ? AND num>= ?", -1, &p_stmt, NULL);
        if( db_error(status, "prepare statement for respond count query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, "binding request-id for respond count query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, start );
        if( db_error(status, "binding start for respond count query", NULL) )
            break;
        status = sqlite3_step( p_stmt );
        if( status == SQLITE_ROW )
            total = sqlite3_column_int( p_stmt, 0 );
        else if( status != SQLITE_DONE ) {
            db_error(status, "respond count query", NULL);
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
        if( db_error(status, "prepare statement for respond query", NULL) )
            break;
        status = sqlite3_bind_text( p_stmt, 1, rid, -1,  SQLITE_STATIC);
        if( db_error(status, "binding request-id for respond query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 2, start );
        if( db_error(status, "binding start for respond query", NULL) )
            break;
        status = sqlite3_bind_int( p_stmt, 3, count );
        if( db_error(status, "binding limit for respond query", NULL) )
            break;
        result = true;
        run = r->respond;
        while( (status = sqlite3_step( p_stmt )) == SQLITE_ROW ) {
            r->stop = sqlite3_column_int( p_stmt, 0 );
            *(run++) = strdup( (char*)sqlite3_column_text(p_stmt, 1) );
        }
        *run = NULL;//array terminator
        if( status!=SQLITE_DONE ) {
            db_error(status, "respond query", NULL);
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

    pthread_mutex_unlock(&db_lock);

    return result;
}

static bool db_error(int status, const char *info, const char *msg) {
    if(status != SQLITE_OK) {
        if(msg!=NULL) {
            gonggo_log("ERROR", "%s is failed with status %d %s (%s)", info, status, sqlite3_errstr(status), msg);
        } else {
            gonggo_log("ERROR", "%s is failed with status %d %s", info, status, sqlite3_errstr(status));
        }
    }
    return status != SQLITE_OK;
}

static void db_close(sqlite3 *p_db) {
    if(p_db!=NULL) {
        sqlite3_close(p_db);
    }
}