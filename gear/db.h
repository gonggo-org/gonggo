#ifndef _DB_H_
#define _DB_H_

#include <stdbool.h>

typedef struct RespondQueryResult {
    char **respond;//respond array terminated by NULL
    int stop;
    int more;
} RespondQueryResult;

extern void db_context_init(const char *gonggo_name, const char *db_path);
extern void db_context_destroy(void);
extern bool db_ensure(void);
extern bool db_respond_purge(long overdue);
extern bool db_respond_insert(const char *rid, const char *respond);

extern void db_respond_query_result_init(RespondQueryResult *r);
extern void db_respond_query_result_destroy(RespondQueryResult *r);
extern bool db_respond_query(const char *rid, int start, int size, RespondQueryResult *r);

#endif //_DB_H_