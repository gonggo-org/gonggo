#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#include "proxyheartbeat.h"
#include "proxythreadtable.h"

#define HEARTBEAT_SUFFIX "_heartbeat"

ProxyHeartbeatThreadData* proxy_heartbeat_thread_data_create(const LogContext *log_ctx, const char *sawang_name,
    long heartbeat_wait, GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table)
{
    ProxyHeartbeatThreadData* thread_data;
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;
    char *heartbeat_path;
    int fd;

    thread_data = (ProxyHeartbeatThreadData*)malloc(sizeof(ProxyHeartbeatThreadData));
    thread_data->heartbeat_wait = heartbeat_wait;
    thread_data->log_ctx = log_ctx;
    strcpy(thread_data->sawang_name, sawang_name);
    thread_data->segment = NULL;
    thread_data->proxy_thread_table = proxy_thread_table;
    thread_data->stop = false;
    thread_data->die = false;
    thread_data->mtx_thread_table = mtx_thread_table;

    pthread_mutexattr_init( &mutexattr );
    pthread_mutexattr_setpshared( &mutexattr, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init( &thread_data->mtx, &mutexattr );
    pthread_mutexattr_destroy( &mutexattr );//mutexattr is no longer needed

    pthread_condattr_init ( &condattr );
    pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_PRIVATE);
    pthread_cond_init( &thread_data->wakeup, &condattr );
    pthread_condattr_destroy( &condattr );//condattr is no longer needed

    heartbeat_path = proxy_heartbeat_path( sawang_name );
    fd = shm_open(heartbeat_path, O_RDWR, S_IRUSR | S_IWUSR);
    thread_data->segment = (ProxyHeartbeatSegmentData*)mmap(NULL, sizeof(ProxyHeartbeatSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    free( heartbeat_path );

    return thread_data;
}

void proxy_heartbeat_thread_data_destroy(ProxyHeartbeatThreadData *data) {
    pthread_mutex_destroy( &data->mtx );
    pthread_cond_destroy( &data->wakeup );
    if( data->segment != NULL ) {
        munmap(data->segment, sizeof(ProxyHeartbeatSegmentData));
        data->segment = NULL;
    }
}

char* proxy_heartbeat_path(const char *sawang_name) {
    char *p;

    p = (char*)malloc(strlen(sawang_name) + strlen(HEARTBEAT_SUFFIX) + 2);
    sprintf(p, "/%s%s", sawang_name, HEARTBEAT_SUFFIX);
    return p;
}

void* proxy_heartbeat_thread(void *arg) {
    ProxyHeartbeatThreadData *p = (ProxyHeartbeatThreadData*)arg;
    struct timespec ts;
    bool heartbeat_timedout;

    gonggo_log(p->log_ctx, "INFO", "proxy %s heartbeat thread starts", p->sawang_name);

    heartbeat_timedout = false;
    while( !p->stop ) {
        heartbeat_timedout = proxy_heart_beat_overdue(p);
        if( heartbeat_timedout )
            break;

        usleep(1000); //1000us = 1ms

        if( pthread_mutex_lock( &p->mtx )==EOWNERDEAD ) {
            gonggo_log(p->log_ctx, "ERROR", "sawang %s left heartbeat mutex inconsistent", p->sawang_name);
            heartbeat_timedout = true;
            break;
        }

        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += p->heartbeat_wait; //wait n seconds from now
        if( pthread_cond_timedwait( &p->wakeup, &p->mtx, &ts )==EOWNERDEAD ) {
            pthread_mutex_consistent( &p->mtx );
            gonggo_log(p->log_ctx, "ERROR", "sawang %s left heartbeat mutex inconsistent", p->sawang_name);
            heartbeat_timedout = true;
            break;
        }

        pthread_mutex_unlock( &p->mtx );
    }

    gonggo_log(p->log_ctx, "INFO", "proxy %s heartbeat thread is stopped", p->sawang_name);
    if( heartbeat_timedout )////proxy removal runs on separate thread to avoid deadlock
        proxy_thread_stop_async(p->log_ctx, p->mtx_thread_table, p->proxy_thread_table, p->sawang_name);

    p->die = true;

    pthread_exit(NULL);
}

bool proxy_heart_beat_overdue(ProxyHeartbeatThreadData *data) {
    time_t overdue;

    pthread_mutex_lock( &data->segment->mtx );
    overdue = data->segment->overdue;
    pthread_mutex_unlock( &data->segment->mtx );
    return time(NULL) > overdue;
}
