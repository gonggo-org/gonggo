#ifndef _CLIENTTIMEOUT_H_
#define _CLIENTTIMEOUT_H_

#include <stdbool.h>

extern void client_timeout_context_init(long period);
extern void client_timeout_context_destroy(void);
extern void* client_timeout(void *arg);
extern void client_timeout_waitfor_started(void);
extern bool client_timeout_isstarted(void);
extern void client_timeout_stop(void);

#endif //_CLIENTTIMEOUT_H_