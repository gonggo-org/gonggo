#ifndef _PROXYCHANNEL_H_
#define _PROXYCHANNEL_H_

#include <stdbool.h>

#include <cJSON.h>

#include "define.h"
#include "proxy.h"

typedef struct {
    int code;
    char *err;
    char *json;    
} RestRespond;

extern ProxyChannelShm *proxy_channel_shm_get(const char *proxy_name);
extern ProxyChannelContext* proxy_channel_context_create(const char* proxy_name, ProxyChannelShm *shm);
extern void proxy_channel_context_destroy(ProxyChannelContext* ctx);
extern void* proxy_channel(void *arg); 
extern void proxy_channel_stop(ProxyChannelContext* ctx);
extern void proxy_channel_rest(ProxyChannelContext* ctx, const char *endpoint, const cJSON *payload, RestRespond* respond);
extern void proxy_channel_rest_respond_destroy(RestRespond* respond);

#endif //_PROXYCHANNEL_H_