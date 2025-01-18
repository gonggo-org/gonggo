#ifndef _PROXYCHANNELTHREADTABLE_H_
#define _PROXYCHANNELTHREADTABLE_H_

#include "proxychannel.h"

typedef struct ProxyChannelTableContext {
    pthread_t thread;
    ProxyChannelContext *ctx;
} ProxyChannelTableContext;

//map proxy-name to ProxyChannelTableContext
extern void proxy_channel_thread_table_create(void);
extern void proxy_channel_thread_table_destroy(void);
extern void proxy_channel_thread_table_set(const char* proxy_name, pthread_t thread, ProxyChannelContext *ctx);
extern const ProxyChannelTableContext* proxy_channel_thread_table_get(const char* proxy_name);
extern void proxy_channel_thread_table_remove(const char* proxy_name);

#endif //_PROXYCHANNELTHREADTABLE_H_

