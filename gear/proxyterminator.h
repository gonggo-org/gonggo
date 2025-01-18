#ifndef _PROXYTERMINATOR_H_
#define _PROXYTERMINATOR_H_

#include <pthread.h>
#include <stdbool.h>

extern void proxy_terminator_context_init(void);
extern void proxy_terminator_context_destroy(void);
extern void* proxy_terminator(void *arg); 
extern void proxy_terminator_waitfor_started(void);
extern bool proxy_terminator_isstarted(void);
extern void proxy_terminator_stop(void);
extern void proxy_terminator_awake(const char *proxy_name);

#endif //_PROXYTERMINATOR_H_