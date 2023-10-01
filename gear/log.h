#ifndef _GONGGO_LOG_H_
#define _GONGGO_LOG_H_

#include <pthread.h>

#define GONGGOLOGBUFLEN 500

typedef struct LogContext {
    pid_t pid;
    const char *path;
    const char *gonggo_name;
    pthread_mutex_t *mtx;
} LogContext;

extern void log_context_init(LogContext *ctx, pid_t pid, const char *path, const char *gonggo_name, pthread_mutex_t *mtx);
extern void log_context_destroy(LogContext *ctx);
extern void gonggo_log(const LogContext *ctx, const char *level, const char *fmt, ...);

#endif //_GONGGO_LOG_H_
