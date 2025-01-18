#ifndef _PROXYALIVE_H_
#define _PROXYALIVE_H_

#include <stdbool.h>

#include "proxy.h"

extern ProxyAliveMutexShm *proxy_alive_shm_get(const char *proxy_name);
extern ProxyAliveContext* proxy_alive_context_create(const char* proxy_name, pid_t pid, ProxyAliveMutexShm *shm);
extern void proxy_alive_context_destroy(ProxyAliveContext* ctx);
extern void* proxy_alive(void *arg); 
extern void proxy_alive_stop(ProxyAliveContext* ctx);

#endif //_PROXYALIVE_H_