#ifndef _PROXYTERMINATEARRAY_H_
#define _PROXYTERMINATEARRAY_H_

#include <glib.h>
#include <stdbool.h>

extern void proxy_terminate_array_create(void);
extern void proxy_terminate_array_destroy(void);
extern void proxy_terminate_array_set(const char *proxy_name);
extern void proxy_terminate_array_drop(const char *proxy_name);
extern bool proxy_terminate_array_exists(const char *proxy_name);

#endif //_PROXYTERMINATEARRAY_H_