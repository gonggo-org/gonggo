#include "globaldata.h"
#include "respondretain.h"

void respondretain_thread_data_init(RespondRetainThreadData *data) {
    data->log_ctx = NULL;
    data->overdue = 0;
    data->period = 0;
    data->db_ctx = 0;
    data->setup = false;
    data->started = false;
}

void respondretain_thread_data_setup(RespondRetainThreadData *data, const LogContext *log_ctx, long overdue, long period, DbContext *db_ctx) {
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    data->log_ctx = log_ctx;
    data->overdue = overdue;
    data->period = period;
    data->db_ctx = db_ctx;

    pthread_mutexattr_init( &mutexattr );
    pthread_mutexattr_setpshared( &mutexattr, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init( &data->mtx, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );//mutexattr is no longer needed

    pthread_condattr_init ( &condattr );
    pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_PRIVATE);
    pthread_cond_init( &data->wakeup, &condattr );
    pthread_condattr_destroy( &condattr );//condattr is no longer needed

    data->setup = true;
    data->started = false;
}

void respondretain_thread_data_destroy(RespondRetainThreadData *data) {
    if( data->setup ) {
        pthread_mutex_destroy(&data->mtx);
        pthread_cond_destroy(&data->wakeup);
        data->setup = false;
    }
}

void respondretain_thread_stop(RespondRetainThreadData *data, pthread_t thread_id) {
    pthread_mutex_lock(&data->mtx);
    pthread_cond_signal(&data->wakeup);
    pthread_mutex_unlock(&data->mtx);
    pthread_join( thread_id, NULL );
}

void *respondretain(void *arg) {
    RespondRetainThreadData *td;
    struct timespec ts;

    td = (RespondRetainThreadData*)arg;
    td->started = true;

    while( !gonggo_exit ) {
        db_respond_retain(td->db_ctx, td->log_ctx, td->overdue);

        pthread_mutex_lock( &td->mtx );
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 5; //wait n seconds from now
        pthread_cond_timedwait( &td->wakeup, &td->mtx, &ts );
        pthread_mutex_unlock( &td->mtx );
    }

    pthread_exit(NULL);
}
