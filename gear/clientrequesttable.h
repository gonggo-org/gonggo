#ifndef _CLIENTREQUESTTABLE_H_
#define _CLIENTREQUESTTABLE_H_

#include <stdbool.h>
#include <glib.h>
#include <civetweb.h>

typedef struct ClientRequestTableContext {
    struct mg_connection* conn;
    time_t timestamp;
    time_t timeoutsec;
} ClientRequestTableContext;

//map request-UUID to ClientRequestTableContext
extern void client_request_table_create(void);
extern void client_request_table_destroy(void);
extern void client_request_table_set(const char* request_uuid, struct mg_connection* conn, time_t timeoutsec);
extern struct mg_connection* client_request_table_get_conn(const char* request_uuid, bool update_time, bool remove_request);
extern void client_request_table_remove(const char* request_uuid);
extern GPtrArray *client_request_table_expired(void);

#endif //_CLIENTREQUESTTABLE_H_