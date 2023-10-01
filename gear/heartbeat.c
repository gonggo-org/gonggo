#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include "globaldata.h"
#include "heartbeat.h"

static HeartbeatData* heartbeat_segment_create(const LogContext *log_ctx, const char *heartbeat_path);
static time_t heartbeat_overdue(float timeout, long heartbeat_wait);

void heartbeat_thread_data_init(HeartbeatThreadData *data) {
    data->log_ctx = NULL;
    data->heartbeat_period = 0;
    data->heartbeat_timeout = 0.0;
    data->gonggo_heartbeat_path = NULL;
    data->segment = NULL;
    data->started = false;
}

void heartbeat_thread_data_setup(HeartbeatThreadData *data, const LogContext *log_ctx,
    long heartbeat_period, float heartbeat_timeout, const char *gonggo_heartbeat_path)
{
    data->log_ctx = log_ctx;
    data->heartbeat_period = heartbeat_period;
    data->heartbeat_timeout = heartbeat_timeout;
    data->gonggo_heartbeat_path = gonggo_heartbeat_path;
    data->segment = heartbeat_segment_create(log_ctx, gonggo_heartbeat_path);
    data->started = false;
}

void heartbeat_thread_data_destroy(HeartbeatThreadData *data) {
    if( data->segment != NULL ) {
        pthread_mutex_destroy( &data->segment->mtx );
        pthread_cond_destroy( &data->segment->wakeup );
        munmap(data->segment, sizeof(HeartbeatData));
        data->segment = NULL;
    }
    if( data->gonggo_heartbeat_path != NULL ) {
        shm_unlink( data->gonggo_heartbeat_path );
        data->gonggo_heartbeat_path = NULL;
    }
}

void heartbeat_thread_stop(HeartbeatThreadData *data, pthread_t thread_id) {
    pthread_mutex_lock(&data->segment->mtx);
    pthread_cond_signal(&data->segment->wakeup);
    pthread_mutex_unlock(&data->segment->mtx);
    pthread_join( thread_id, NULL );
}

void* heartbeat(void *arg) {
    HeartbeatThreadData *td = (HeartbeatThreadData*)arg;
    struct timespec ts;

    gonggo_log(td->log_ctx, "INFO", "heartbeat thread is started");

    td->segment->overdue = heartbeat_overdue(td->heartbeat_timeout, td->heartbeat_period);

    td->started = true;

    while( !gonggo_exit ) {
        usleep(1000); //1000us = 1ms

        while( pthread_mutex_lock( &td->segment->mtx )==EOWNERDEAD )
            pthread_mutex_consistent( &td->segment->mtx );

        td->segment->overdue = heartbeat_overdue(td->heartbeat_timeout, td->heartbeat_period);

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += td->heartbeat_period; //wait n seconds from now
        if( pthread_cond_timedwait( &td->segment->wakeup, &td->segment->mtx, &ts )==EOWNERDEAD ) {
            do {
                pthread_mutex_consistent( &td->segment->mtx );
            } while(pthread_mutex_lock( &td->segment->mtx )==EOWNERDEAD);
        }

        pthread_mutex_unlock( &td->segment->mtx );
    }

    gonggo_log(td->log_ctx, "INFO", "heartbeat thread is stopped");
    pthread_exit(NULL);
}

static HeartbeatData* heartbeat_segment_create(const LogContext *log_ctx, const char *heartbeat_path) {
    int fd;
    char buff[GONGGOLOGBUFLEN];
    HeartbeatData* map;
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    fd = shm_open(heartbeat_path, O_RDWR, S_IRUSR | S_IWUSR);
    if( errno == ENOENT ) {
        fd = shm_open(heartbeat_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if(fd>-1)
            ftruncate(fd, sizeof(HeartbeatData));
    }

    if(fd==-1) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log(log_ctx, "ERROR", "heartbeat %s creation failed, %s", heartbeat_path, buff);
        return NULL;
    }

    map = (HeartbeatData*)mmap(NULL, sizeof(HeartbeatData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    pthread_mutexattr_init( &mutexattr );
    pthread_mutexattr_setpshared( &mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust( &mutexattr, PTHREAD_MUTEX_ROBUST );
    if(pthread_mutex_init( &map->mtx, &mutexattr ) == EBUSY) {
        if( pthread_mutex_consistent(&map->mtx)==0 )
            pthread_mutex_unlock(&map->mtx);
    }
    pthread_mutexattr_destroy( &mutexattr );//mutexattr is no longer needed

    pthread_condattr_init ( &condattr );
    pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init( &map->wakeup, &condattr );
    pthread_condattr_destroy( &condattr );//condattr is no longer needed

    return map;
}

static time_t heartbeat_overdue(float timeout, long heartbeat_wait) {
    time_t offset;

    offset = timeout * heartbeat_wait;
    return time(NULL) + offset;
}
