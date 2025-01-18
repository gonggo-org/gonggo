#ifndef _PROXYCHANNEL_H_
#define _PROXYCHANNEL_H_

#include <stdbool.h>

#include "define.h"
#include "proxy.h"

extern ProxyChannelShm *proxy_channel_shm_get(const char *proxy_name);
extern ProxyChannelContext* proxy_channel_context_create(const char* proxy_name, ProxyChannelShm *shm);
extern void proxy_channel_context_destroy(ProxyChannelContext* ctx);
extern void* proxy_channel(void *arg); 
extern void proxy_channel_stop(ProxyChannelContext* ctx);

#endif //_PROXYCHANNEL_H_