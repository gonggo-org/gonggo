#ifndef _CLIENTPROXYNAMETABLE_H_
#define _CLIENTPROXYNAMETABLE_H_

#include <stdbool.h>
#include <glib.h>

typedef struct ClientProxynameTableValue {
    GPtrArray *request_uuids;//array of string
    GArray *sents;//array of boolean
} ClientProxynameTableValue;

//map proxy-name to ClientProxynameTableValue
extern void client_proxyname_table_create(void);
extern GHashTable *client_proxyname_table_similar(void);
extern void client_proxyname_table_destroy(void);
extern void client_proxyname_table_set(const char *proxy_name, const char *request_uuid, GHashTable *other_table_nolock);
extern void client_proxyname_table_request_sent(const char *proxy_name, const char *request_uuid);
extern void client_proxyname_table_request_unsent(const char *proxy_name);
extern bool client_proxyname_table_drop(const char* proxy_name, const char* request_uuid);
extern void client_proxyname_table_drop_all(const char* request_uuid);//search in all proxy_names
extern GPtrArray* client_proxyname_table_request_dup(const char* proxy_name, bool not_sent_only);
extern char *client_proxyname_table_proxy_name_dup(const char *request_uuid);

#endif //_CLIENTPROXYNAMETABLE_H_