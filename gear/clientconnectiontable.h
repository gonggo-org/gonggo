#ifndef _CLIENTCONNECTIONTABLE_H_
#define _CLIENTCONNECTIONTABLE_H_

#include <civetweb.h>

//map connection object to request-UUID array
extern void client_connection_table_create(void);
extern GHashTable *client_connection_table_similar(void);
extern void client_connection_table_destroy(void);
extern void client_connection_table_set(struct mg_connection* conn, const char* request_uuid, GHashTable *othertable_nolock);
extern void client_connection_table_drop(struct mg_connection* conn, const char* request_uuid);
extern GPtrArray* client_connection_table_request_dup(const struct mg_connection* conn);
extern void client_connection_table_remove(const struct mg_connection* conn);

#endif //_CLIENTCONNECTIONTABLE_H_