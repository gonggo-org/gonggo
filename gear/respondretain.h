#ifndef _RESPONDRETAIN_H_
#define _RESPONDRETAIN_H_

#include "db.h"
#include "log.h"

typedef struct RespondRetainThreadData {
    const LogContext *log_ctx;
    long overdue;
    long period;
    DbContext *db_ctx;
    pthread_mutex_t mtx;
    pthread_cond_t wakeup;
    bool setup;
    volatile bool started;
} RespondRetainThreadData;

extern void respondretain_thread_data_init(RespondRetainThreadData *data);
extern void respondretain_thread_data_setup(RespondRetainThreadData *data, const LogContext *log_ctx, long overdue, long period, DbContext *db_ctx);
extern void respondretain_thread_data_destroy(RespondRetainThreadData *data);
extern void respondretain_thread_stop(RespondRetainThreadData *data, pthread_t thread_id);
extern void *respondretain(void *arg);

#endif //_RESPONDRETAIN_H_
