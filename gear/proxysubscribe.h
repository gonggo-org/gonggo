#ifndef _PROXYSUBSCRIBE_H_
#define _PROXYSUBSCRIBE_H_

#include <stdbool.h>

#include "define.h"
#include "proxy.h"

extern ProxySubscribeShm *proxy_subscribe_shm_get(const char *proxy_name);
extern ProxySubscribeContext* proxy_subscribe_context_create(const char* proxy_name, ProxySubscribeShm *shm);
extern void proxy_subscribe_context_destroy(ProxySubscribeContext* ctx);
extern void* proxy_subscribe(void *arg); 
extern void proxy_subscribe_stop(ProxySubscribeContext* ctx);

#endif //_PROXYSUBSCRIBE_H_