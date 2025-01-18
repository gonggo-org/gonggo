#ifndef _CLIENTSERVICE_H_
#define _CLIENTSERVICE_H_

#include <civetweb.h>

extern void client_service_route(const char *remote_addr, const char *uri, const char *data, size_t datasize, struct mg_connection *conn);
extern void client_service_drop_conn(const struct mg_connection *conn);

#endif //_CLIENTSERVICE_H_