#ifndef _PROXYSUBSCRIBETHREADTABLE_H_
#define _PROXYSUBSCRIBETHREADTABLE_H_

#include <glib.h>

#include "proxysubscribe.h"

typedef struct ProxySubscribeTableContext {
    pthread_t thread;
    ProxySubscribeContext *ctx;
} ProxySubscribeTableContext;

//map proxy-name to ProxySubscribeTableContext
extern void proxy_subscribe_thread_table_create(void);
extern void proxy_subscribe_thread_table_destroy(void);
extern void proxy_subscribe_thread_table_set(const char* proxy_name, pthread_t thread, ProxySubscribeContext *ctx);
extern const ProxySubscribeTableContext* proxy_subscribe_thread_table_get(const char* proxy_name);
extern void proxy_subscribe_thread_table_remove(const char* proxy_name);

#endif //_PROXYSUBSCRIBETHREADTABLE_H_

