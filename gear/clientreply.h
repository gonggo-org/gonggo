#ifndef _REPLY_H_
#define _REPLY_H_

#include <civetweb.h>
#include <glib.h>

#include "cJSON.h"
#include "servicestatus.h"

extern void client_proxy_alive_notification(const char *proxy_name, bool started);
extern void client_parsing_status_reply(struct mg_connection *conn, const char *rid, enum ServiceStatus status);
extern void client_service_reply(struct mg_connection *conn, const char *rid, cJSON* proxy_headers, cJSON* proxy_payload, bool db_save);
extern void client_service_expired_reply(struct mg_connection *conn, const char *rid, bool db_save);    

#endif //_REPLY_H_
