#ifndef _PROXYALIVETHREADTABLE_H_
#define _PROXYALIVETHREADTABLE_H_

#include <glib.h>

#include "proxy.h"
#include "strarr.h"

typedef struct ProxyAliveTableContext {
    pthread_t thread;
    ProxyAliveContext *ctx;
} ProxyAliveTableContext;

//map proxy-name to ProxyAliveTableContext
extern void proxy_alive_thread_table_create(void);
extern void proxy_alive_thread_table_destroy(void);
extern void proxy_alive_thread_table_set(const char* proxy_name, pthread_t thread, ProxyAliveContext *ctx);
extern const ProxyAliveTableContext* proxy_alive_thread_table_get(const char* proxy_name);
extern void proxy_alive_thread_table_remove(const char* proxy_name);
extern void proxy_alive_thread_table_char_keys_create(StrArr *sa);

#endif //_PROXYALIVETHREADTABLE_H_