#ifndef _PROXYACTIVATOR_H_
#define _PROXYACTIVATOR_H_

#include <stdbool.h>

extern bool proxy_activator_context_init(void);
extern void proxy_activator_context_destroy(void);
extern void* proxy_activator(void *arg);
extern void proxy_activator_waitfor_started(void);
extern bool proxy_activator_isstarted(void);
extern void proxy_activator_stop(void);
extern const char *proxy_activator_get_gonggo_path(void);

#endif //_PROXYACTIVATOR_H_