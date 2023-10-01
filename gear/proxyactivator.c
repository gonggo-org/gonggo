#include <pthread.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <glib.h>
#include <stdio.h>
#include <errno.h>

#include "proxythreadtable.h"
#include "proxyactivator.h"
#include "globaldata.h"
#include "log.h"
#include "proxysubscribe.h"
#include "proxychannel.h"
#include "proxyheartbeat.h"

static void activation_data_init(ActivationData *data);
static void activation(
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    ActivationData *pshm, const LogContext *log_ctx, long heartbeat_period,
    pthread_attr_t *thread_attr, const DbContext *db_ctx);
static void deactivation(GHashTable *proxy_thread_table, ActivationData *pshm, const LogContext *log_ctx, pthread_mutex_t *mtx_thread_table);
static bool proxy_shm_test(const LogContext *log_ctx, const char *sawang_name, enum ProxySegment proxy_segment);
static ActivationData* activation_create(const LogContext *log_ctx, const char *gonggo_path);

void proxy_activation_thread_data_init(ProxyActivationThreadData *data) {
    data->log_ctx = NULL;
    data->gonggo_path = NULL;
    data->request_table = NULL;
    data->mtx_request_table = NULL;
    data->proxy_thread_table = NULL;
    data->mtx_thread_table = NULL;
    data->heartbeat_period = 0;
    data->segment = NULL;
    data->db_ctx = NULL;
    data->started = false;
}

void proxy_activation_thread_data_setup(ProxyActivationThreadData *data,
    const LogContext *log_ctx, const char *gonggo_path,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    long heartbeat_period,const DbContext *db_ctx)
{
    data->log_ctx = log_ctx;
    data->gonggo_path = gonggo_path;
    data->request_table = request_table;
    data->mtx_request_table = mtx_request_table;
    data->proxy_thread_table = proxy_thread_table;
    data->mtx_thread_table = mtx_thread_table;
    data->heartbeat_period = heartbeat_period;
    data->segment = activation_create(log_ctx, gonggo_path);
    data->db_ctx = db_ctx;
    data->started = false;
}

void proxy_activation_thread_stop(ProxyActivationThreadData *data, pthread_t thread_id) {
    pthread_mutex_lock(&data->segment->mtx);
    if( data->segment->state == ACTIVATION_IDLE ) {
        pthread_cond_signal(&data->segment->cond_dispatcher_wakeup);
    }
    pthread_mutex_unlock(&data->segment->mtx);
    pthread_join( thread_id, NULL );
}

void proxy_activation_thread_data_destroy(ProxyActivationThreadData *data) {
    if( data->segment != NULL ) {
        pthread_mutex_destroy( &data->segment->mtx );
        pthread_cond_destroy( &data->segment->cond_idle );
        pthread_cond_destroy( &data->segment->cond_dispatcher_wakeup );
        pthread_cond_destroy( &data->segment->cond_proxy_wakeup );
        munmap(data->segment, sizeof(ActivationData));
        data->segment = NULL;
    }
    if( data->gonggo_path != NULL ) {
        shm_unlink( data->gonggo_path );
        data->gonggo_path = NULL;
    }
}

void* proxy_activation_listener(void *arg) {
    ProxyActivationThreadData *td = (ProxyActivationThreadData*)arg;
    struct timespec ts;
    pthread_attr_t thread_attr;
    GList *keys, *run;
    const char *sawang_name;
    bool died;

    gonggo_log(td->log_ctx, "INFO", "activation listener thread is started");

    activation_data_init(td->segment);

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    td->started = true;

    while( !gonggo_exit ) {
        while( pthread_mutex_lock( &td->segment->mtx )==EOWNERDEAD )
            pthread_mutex_consistent( &td->segment->mtx );

        died = false;
        if( td->segment->state == ACTIVATION_IDLE ) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5; //wait 5 seconds from now
            if( pthread_cond_timedwait( &td->segment->cond_dispatcher_wakeup, &td->segment->mtx, &ts )==EOWNERDEAD ) {//proxy died
                died = true;
                do {
                    pthread_mutex_consistent( &td->segment->mtx );
                } while( pthread_mutex_lock( &td->segment->mtx )==EOWNERDEAD );
            }
        }

        if( !died
            && strlen(td->segment->sawang_name) > 0
            && (td->segment->state == ACTIVATION_REQUEST || td->segment->state == DEACTIVATION_REQUEST)
        ) {
            if( td->segment->state == ACTIVATION_REQUEST )
                activation(
                    td->proxy_thread_table, td->mtx_thread_table,
                    td->request_table, td->mtx_request_table,
                    td->segment, td->log_ctx, td->heartbeat_period,
                    &thread_attr, td->db_ctx);
            else
                deactivation(td->proxy_thread_table, td->segment, td->log_ctx, td->mtx_thread_table);
        }

        activation_data_init(td->segment);
        pthread_cond_signal(&td->segment->cond_idle);
        pthread_mutex_unlock( &td->segment->mtx );

        usleep(1000); //1000us = 1ms
    }

    pthread_mutex_lock(td->mtx_thread_table);
    keys = g_hash_table_get_keys(td->proxy_thread_table);
    if(keys!=NULL) {
        run = keys;
        while( run != NULL ) {
            sawang_name = (const char*)run->data;
            gonggo_log(td->log_ctx, "INFO", "proxy %s stop starts", sawang_name);
            proxy_channel_stop(sawang_name, td->log_ctx);
            proxy_remove(td->log_ctx, td->proxy_thread_table, sawang_name);
            run = g_list_next(run);
        }
        g_list_free(keys);
    }
    pthread_mutex_unlock(td->mtx_thread_table);

    pthread_attr_destroy(&thread_attr);

    gonggo_log(td->log_ctx, "INFO", "activation listener thread is stopped");

    pthread_exit(NULL);
}

static void activation_data_init(ActivationData *data) {
    data->sawang_name[0] = 0;
    data->state = ACTIVATION_IDLE;
    return;
}

static void activation(
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    ActivationData *pshm, const LogContext *log_ctx, long heartbeat_period,
    pthread_attr_t *thread_attr, const DbContext *db_ctx)
{
    ProxySubscribeThreadData *proxy_subscribe_thread_data;
    ProxyHeartbeatThreadData *proxy_heartbeat_thread_data;
    pthread_t t_subscribe_thread, t_heartbeat_thread;
    struct timespec ts;
    int wait_status;

    proxy_subscribe_thread_data = NULL;
    proxy_heartbeat_thread_data = NULL;
    do {
        gonggo_log(log_ctx, "INFO", "remove existing thread for proxy %s", pshm->sawang_name);
        proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, pshm->sawang_name);
        gonggo_log(log_ctx, "INFO", "existing thread for proxy %s has been removed", pshm->sawang_name);

        //test channel shm
        if( !proxy_shm_test(log_ctx, pshm->sawang_name, CHANNEL) ) {
            pshm->state = ACTIVATION_FAILED;
            break;
        }

        //test subscribe shm
        if( !proxy_shm_test(log_ctx, pshm->sawang_name, SUBSCRIBE) ) {
            pshm->state = ACTIVATION_FAILED;
            break;
        }

        //test heartbeat shm
        if( !proxy_shm_test(log_ctx, pshm->sawang_name, HEARTBEAT) ) {
            pshm->state = ACTIVATION_FAILED;
            break;
        }

        gonggo_log(log_ctx, "INFO", "create subscribe thread for proxy %s", pshm->sawang_name);
        proxy_subscribe_thread_data = proxy_subscribe_thread_data_create(
            log_ctx, request_table, mtx_request_table,
            pshm->sawang_name, db_ctx,
            proxy_thread_table, mtx_thread_table);
        if( pthread_create( &t_subscribe_thread, thread_attr, proxy_subscribe_thread, proxy_subscribe_thread_data ) != 0 ) {
            pshm->state = ACTIVATION_FAILED;
            free(proxy_subscribe_thread_data);
            proxy_subscribe_thread_data = NULL;
            gonggo_log(log_ctx, "ERROR", "cannot create subscriber thread for proxy %s", pshm->sawang_name);
            break;
        }

        gonggo_log(log_ctx, "INFO", "create heartbeat thread for proxy %s", pshm->sawang_name);
        proxy_heartbeat_thread_data = proxy_heartbeat_thread_data_create(log_ctx, pshm->sawang_name,
            pshm->proxy_heartbeat, proxy_thread_table, mtx_thread_table);
        if( pthread_create( &t_heartbeat_thread, thread_attr, proxy_heartbeat_thread, proxy_heartbeat_thread_data) != 0 ) {
            pshm->state = ACTIVATION_FAILED;
            free(proxy_subscribe_thread_data);
            proxy_subscribe_thread_data = NULL;
            free(proxy_heartbeat_thread_data);
            proxy_heartbeat_thread_data = NULL;
            gonggo_log(log_ctx, "ERROR", "cannot create heartbeat thread for proxy %s", pshm->sawang_name);
            break;
        }

        pshm->state = ACTIVATION_SUCCESS;
        pshm->dispatcher_heartbeat = heartbeat_period;
        gonggo_log(log_ctx, "INFO", "subscriber and heartbeat thread for proxy %s is started", pshm->sawang_name);
    } while(false);

    pthread_cond_signal(&pshm->cond_proxy_wakeup);
    pthread_mutex_unlock( &pshm->mtx );
    usleep(1000);

    if( pthread_mutex_lock( &pshm->mtx )==EOWNERDEAD ) {
        pthread_mutex_consistent( &pshm->mtx );
        pthread_mutex_lock( &pshm->mtx );
        gonggo_log(log_ctx, "ERROR", "sawang %s left mutex inconsistent during activation", pshm->sawang_name);
        proxy_thread_stop(t_subscribe_thread, proxy_subscribe_thread_data,
            t_heartbeat_thread, proxy_heartbeat_thread_data,
            true);
        return;
    }

    wait_status = 0;
    if( pshm->state != ACTIVATION_DONE ) {
        ts.tv_sec += 10; //wait 10 seconds from now
        clock_gettime(CLOCK_REALTIME, &ts);
        if( (wait_status = pthread_cond_timedwait( &pshm->cond_dispatcher_wakeup, &pshm->mtx, &ts))==EOWNERDEAD ) {
            pthread_mutex_consistent(&pshm->mtx);
            pthread_mutex_lock(&pshm->mtx);
            gonggo_log(log_ctx, "ERROR", "sawang %s left mutex inconsistent during activation", pshm->sawang_name);
            proxy_thread_stop(t_subscribe_thread, proxy_subscribe_thread_data,
                t_heartbeat_thread, proxy_heartbeat_thread_data,
                true);
            return;
        }
    }

    if( pshm->state == ACTIVATION_DONE ) {
        if( proxy_subscribe_thread_data != NULL ) {
            proxy_register(mtx_thread_table, proxy_thread_table, pshm->sawang_name,
                t_subscribe_thread, proxy_subscribe_thread_data,
                t_heartbeat_thread, proxy_heartbeat_thread_data);
        }
    } else {
        if(wait_status == ETIMEDOUT)
            gonggo_log(log_ctx, "ERROR", "times out on waiting ACTIVATION DONE response from proxy %s", pshm->sawang_name);
        else
            gonggo_log(log_ctx, "ERROR", "state %d is different from expected ACTIVATION DONE response from proxy %s",
                pshm->state, pshm->sawang_name);
        proxy_thread_stop(t_subscribe_thread, proxy_subscribe_thread_data,
            t_heartbeat_thread, proxy_heartbeat_thread_data,
            true);
    }

    return;
}

static void deactivation(GHashTable *proxy_thread_table, ActivationData *pshm, const LogContext *log_ctx, pthread_mutex_t *mtx_thread_table) {
    struct timespec ts;
    int wait_status;

    gonggo_log(log_ctx, "INFO", "deactivation for proxy %s is started", pshm->sawang_name);
    do {
        if( !proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, pshm->sawang_name) ) {
            pshm->state = DEACTIVATION_NOTEXISTS;
            gonggo_log(log_ctx, "INFO", "thread for proxy %s is not available", pshm->sawang_name);
            break;
        }
        gonggo_log(log_ctx, "INFO", "thread for proxy %s is stopped", pshm->sawang_name);
        pshm->state = DEACTIVATION_SUCCESS;
    } while(false);

    pthread_cond_signal(&pshm->cond_proxy_wakeup);
    pthread_mutex_unlock(&pshm->mtx);
    usleep(1000);

    if( pthread_mutex_lock( &pshm->mtx )==EOWNERDEAD ) {
        pthread_mutex_consistent( &pshm->mtx );
        pthread_mutex_lock( &pshm->mtx );
        gonggo_log(log_ctx, "ERROR", "sawang %s left activation mutex inconsistent during deactivation", pshm->sawang_name);
        return;
    }

    wait_status = 0;
    if( pshm->state != DEACTIVATION_DONE ) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += 10; //wait 10 seconds from now
        if( (wait_status = pthread_cond_timedwait( &pshm->cond_dispatcher_wakeup, &pshm->mtx, &ts))==EOWNERDEAD ) {
            pthread_mutex_consistent(&pshm->mtx);
            pthread_mutex_lock(&pshm->mtx);
            gonggo_log(log_ctx, "ERROR", "sawang %s left activation mutex inconsistent during deactivation", pshm->sawang_name);
            return;
        }
    }

    if( pshm->state != DEACTIVATION_DONE ) {
        if(wait_status == ETIMEDOUT)
            gonggo_log(log_ctx, "ERROR", "times out on waiting DEACTIVATION DONE response from proxy %s", pshm->sawang_name);
        else
            gonggo_log(log_ctx, "ERROR", "state %d is different from expected DEACTIVATION DONE response from proxy %s",
                pshm->state, pshm->sawang_name);
    }

    return;
}

static bool proxy_shm_test(const LogContext *log_ctx, const char *sawang_name, enum ProxySegment proxy_segment) {
    char *p;
    int fd;
    char buff[GONGGOLOGBUFLEN];
    bool status;
    ChannelSegmentData *p_channel;
    ProxySubscribeSegmentData *p_subscribe;
    ProxyHeartbeatSegmentData *p_heartbeat;

    p = NULL;
    status = false;
    fd = -1;
    do {
        if( proxy_segment == CHANNEL ) {
            p = proxy_channel_path(sawang_name);
            fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
            if(fd==-1) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            p_channel = (ChannelSegmentData*)mmap(NULL, sizeof(ChannelSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if( p_channel == MAP_FAILED ) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            munmap(p_channel, sizeof(ChannelSegmentData));
        } else if( proxy_segment == SUBSCRIBE ) {
            p = proxy_subscribe_path(sawang_name);
            fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
            if(fd==-1) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            p_subscribe = (ProxySubscribeSegmentData*)mmap(NULL, sizeof(ProxySubscribeSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if( p_subscribe == MAP_FAILED ) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            munmap(p_subscribe, sizeof(ProxySubscribeSegmentData));
        } else if( proxy_segment == HEARTBEAT ) {
            p = proxy_heartbeat_path(sawang_name);
            fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
            if(fd==-1) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            p_heartbeat = (ProxyHeartbeatSegmentData*)mmap(NULL, sizeof(ProxyHeartbeatSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if( p_heartbeat == MAP_FAILED ) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR",
                    "shared memory access failed for segment %s of proxy %s, %s",
                    proxy_segment_label(proxy_segment), p, buff);
                break;
            }
            munmap(p_heartbeat, sizeof(ProxyHeartbeatSegmentData));
        }

        status = true;
    } while (false);

    if(fd>-1)
        close(fd);

    if(p!=NULL)
        free(p);

    return status;
}

static ActivationData* activation_create(const LogContext *log_ctx, const char *gonggo_path) {
    int fd;
    char buff[GONGGOLOGBUFLEN];
    ActivationData* map;
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    fd = shm_open(gonggo_path, O_RDWR, S_IRUSR | S_IWUSR);
    if( errno == ENOENT ) {
        fd = shm_open(gonggo_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if(fd>-1)
            ftruncate(fd, sizeof(ActivationData));
    }

    if(fd==-1) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log(log_ctx, "ERROR", "shm %s creation failed, %s", gonggo_path, buff);
        return NULL;
    }

    map = (ActivationData*)mmap(NULL, sizeof(ActivationData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);

    pthread_mutexattr_init( &mutexattr );
    pthread_mutexattr_setpshared( &mutexattr, PTHREAD_PROCESS_SHARED );
    pthread_mutexattr_setrobust( &mutexattr, PTHREAD_MUTEX_ROBUST );
    if(pthread_mutex_init( &map->mtx, &mutexattr ) == EBUSY) {
        if( pthread_mutex_consistent(&map->mtx)==0 )
            pthread_mutex_unlock(&map->mtx);
    }
    pthread_mutexattr_destroy( &mutexattr );//mutexattr is no longer needed

    pthread_condattr_init ( &condattr );
    pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init( &map->cond_idle, &condattr );
    pthread_cond_init( &map->cond_dispatcher_wakeup, &condattr );
    pthread_cond_init( &map->cond_proxy_wakeup, &condattr );
    pthread_condattr_destroy( &condattr );//condattr is no longer needed

    return map;
}
