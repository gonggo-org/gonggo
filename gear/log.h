#ifndef _GONGGO_LOG_H_
#define _GONGGO_LOG_H_

#include <pthread.h>

extern void gonggo_log_context_init(pid_t pid, const char *path);
extern void gonggo_log_context_destroy(void);
extern void gonggo_log(const char *level, const char *fmt, ...);

#endif //_GONGGO_LOG_H_