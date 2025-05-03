#ifndef _CLIENTSERVICETABLE_H_
#define _CLIENTSERVICETABLE_H_

#include <glib.h>

#include <cJSON.h>

#include "proxy.h"

//map request-UUID to service_name_payload json {"service":"test", "payload":{"key1":"value1", "key2":"value2"}}
extern void client_service_table_create(void);
extern void client_service_table_destroy(void);
extern char* client_service_table_service_payload_create(const char *service_name, const cJSON *payload);
extern void client_service_table_set(const char* request_uuid, const char *service_name, const cJSON *payload);
extern char* client_service_table_dup(const char* request_uuid);
extern void client_service_table_remove(const char* request_uuid);

#endif //_CLIENTSERVICETABLE_H_