#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>
#include <errno.h>

#include "proxychannel.h"
#include "reply.h"
#include "servicestatus.h"
#include "proxythreadtable.h"
#include "proxyheartbeat.h"

#define CHANNEL_SUFFIX "_channel"
#define REMOVE_REQUEST_SUFFIX "_requestremove"

static bool channel_conversation(const char *rid, const LogContext *log_ctx,
    struct mg_connection *conn,
    const char *sawang_name, const char *task, const cJSON *payload, ChannelSegmentData *pshm,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);
static bool channel_request(const char *rid, const char *task,
    const char *payload_str, int payload_buff_length,
    const LogContext *log_ctx, struct mg_connection *conn,
    char **payload_path);

static bool channel_dead_request_conversation(const char *sawang_name, const LogContext *log_ctx, GSList *rid_list,
    ChannelSegmentData *pshm, GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table);
static bool channel_dead_request_payload(const char *sawang_name, const LogContext *log_ctx, GSList *rid_list,
    char **payload_path, unsigned int *payload_buff_length);

char* proxy_channel_path(const char *sawang_name) {
    char *channel_path;

    channel_path = (char*)malloc(strlen(sawang_name) + strlen(CHANNEL_SUFFIX) + 2);
    sprintf(channel_path, "/%s%s", sawang_name, CHANNEL_SUFFIX);
    return channel_path;
}

bool proxy_channel(const char *rid, const LogContext *log_ctx,
    struct mg_connection *conn,
    const char *sawang_name, const char *task, const cJSON *arg,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table) {

    char *channel_path, buff[GONGGOLOGBUFLEN];
    int fd, ttl;
    bool success, done;
    ChannelSegmentData *pshm;
    struct timespec ts;

    ttl = 5;
    channel_path = proxy_channel_path(sawang_name);
    fd = -1;
    success = false;

    do {
        if( !proxy_exists(mtx_thread_table, proxy_thread_table, sawang_name) ) {
            gonggo_log(log_ctx, "ERROR", "proxy %s is not available", sawang_name);
            reply_status(conn, rid, SERVICE_STATUS_PROXY_NOT_AVAILABLE);
            break;
        }

        fd = shm_open(channel_path, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "proxy %s, task %s, open shared memory %s failed, %s",
                sawang_name, task, channel_path, buff);
            reply_status(conn, rid, SERVICE_STATUS_PROXY_NOT_AVAILABLE);
            break;
        }

        pshm = (ChannelSegmentData*)mmap(NULL, sizeof(ChannelSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if( pshm == MAP_FAILED ) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "proxy %s task %s, map shared memory %s failed, %s",
                sawang_name, task, channel_path, buff);
            reply_status(conn, rid, SERVICE_STATUS_PROXY_NOT_ACCESSIBLE);
            break;
        }

        done = false;
        while( !done && ttl-- > 0 ) {
            if( pthread_mutex_lock( &pshm->mtx )==EOWNERDEAD ) {
                proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_DIED);
                break;
            }

            if( pshm->state != CHANNEL_IDLE ) {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 5; //wait 5 seconds from now
                if( pthread_cond_timedwait( &pshm->cond_idle, &pshm->mtx, &ts )==EOWNERDEAD ) {
                    pthread_mutex_consistent( &pshm->mtx );
                    proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                    gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                    reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_DIED);
                    break;
                }
            }

            if( pshm->state == CHANNEL_IDLE ) {
                success = channel_conversation(rid, log_ctx, conn, sawang_name, task, arg, pshm,
                    proxy_thread_table, mtx_thread_table);
                done = true;
            } else
                pthread_mutex_unlock( &pshm->mtx );

            if( !done ) {
                if( ttl < 1 ) {
                    gonggo_log(log_ctx, "ERROR", "proxy %s is not idle for task %s", sawang_name, task);
                    reply_status(conn, rid, SERVICE_STATUS_PROXY_BUSY);
                    break;
                }
                usleep(1000); //1000us = 1ms
            }
        }//while( !done && ttl-- > 0 )

        munmap(pshm, sizeof(ChannelSegmentData));
    } while(false);

    if(fd>-1)
        close(fd);
    free(channel_path);

    return success;
}

void proxy_channel_stop(const char *sawang_name, const LogContext *log_ctx) {
    char *channel_path, buff[GONGGOLOGBUFLEN];
    int fd, ttl;
    ChannelSegmentData *pshm;
    bool done;
    struct timespec ts;

    channel_path = proxy_channel_path(sawang_name);
    pshm = NULL;
    fd = -1;
    ttl = 5;
    done = false;

    do {
        fd = shm_open(channel_path, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "opening shared memory %s for proxy %s stopping is failed, %s",
                channel_path, sawang_name, buff);
            break;
        }

        pshm = (ChannelSegmentData*)mmap(NULL, sizeof(ChannelSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if( pshm == MAP_FAILED ) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "accessing shared memory %s for proxy %s stopping is failed, %s",
                channel_path, sawang_name, buff);
            break;
        }

        done = false;
        while( !done && ttl-- > 0 ) {
            if( pthread_mutex_lock( &pshm->mtx )==EOWNERDEAD ) {
                gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                break;
            }

            if( pshm->state != CHANNEL_IDLE ) {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 5; //wait 5 seconds from now
                if( pthread_cond_timedwait( &pshm->cond_idle, &pshm->mtx, &ts )==EOWNERDEAD ) {
                    pthread_mutex_consistent(&pshm->mtx);
                    gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                    break;
                }
            }

            if( pshm->state == CHANNEL_IDLE ) {
                pshm->state = CHANNEL_STOP;
                pthread_cond_signal(&pshm->cond_proxy_wakeup);
                done = true;
            }

            pthread_mutex_unlock( &pshm->mtx );

            if( !done ) {
                if( ttl < 1 ) {
                    gonggo_log(log_ctx, "ERROR", "proxy %s is not idle for stopping", sawang_name);
                    break;
                }
                usleep(1000); //1000us = 1ms
            }
        }

        munmap(pshm, sizeof(ChannelSegmentData));
    } while(false);

    free(channel_path);
    if(fd>-1)
        close(fd);

    return;
}

void proxy_channel_dead_request(const char *sawang_name, GSList *rid_list, const LogContext *log_ctx,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table)
{
    char *channel_path, buff[GONGGOLOGBUFLEN];
    int fd, ttl;
    bool done;
    ChannelSegmentData *pshm;
    struct timespec ts;

    ttl = 5;
    channel_path = proxy_channel_path(sawang_name);
    fd = -1;

    do {
        if( !proxy_exists(mtx_thread_table, proxy_thread_table, sawang_name) ) {
            gonggo_log(log_ctx, "ERROR", "proxy %s is not available", sawang_name);
            break;
        }

        fd = shm_open(channel_path, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "remove request from proxy %s, open shared memory %s failed, %s",
                sawang_name, channel_path, buff);
            break;
        }

        pshm = (ChannelSegmentData*)mmap(NULL, sizeof(ChannelSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if( pshm == MAP_FAILED ) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "remove request from proxy %s, map shared memory %s failed, %s",
                sawang_name, channel_path, buff);
            break;
        }

        done = false;
        while( !done && ttl-- > 0 ) {
            if( pthread_mutex_lock( &pshm->mtx )==EOWNERDEAD ) {
                proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                break;
            }

            if( pshm->state != CHANNEL_IDLE ) {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 5; //wait 5 seconds from now
                if( pthread_cond_timedwait( &pshm->cond_idle, &pshm->mtx, &ts )==EOWNERDEAD ) {
                    pthread_mutex_consistent( &pshm->mtx );
                    proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                    gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
                    break;
                }
            }

            if( pshm->state == CHANNEL_IDLE ) {
                channel_dead_request_conversation(sawang_name, log_ctx, rid_list,
                    pshm, proxy_thread_table, mtx_thread_table);
                done = true;
            } else
                pthread_mutex_unlock( &pshm->mtx );

            if( !done ) {
                if( ttl < 1 ) {
                    gonggo_log(log_ctx, "ERROR", "proxy %s is not idle for request removal", sawang_name);
                    break;
                }
                usleep(1000); //1000us = 1ms
            }
        }//while( !done && ttl-- > 0 )

        munmap(pshm, sizeof(ChannelSegmentData));
    } while(false);

    if(fd>-1)
        close(fd);
    free(channel_path);
}

static bool channel_conversation(const char *rid, const LogContext *log_ctx,
    struct mg_connection *conn,
    const char *sawang_name, const char *task, const cJSON *arg, ChannelSegmentData *pshm,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table)
{
    char *payload_str, *payload_path;
    int payload_buff_length, ttl;
    bool acknowledged, busy, died;
    struct timespec ts;

    ttl = 5;
    acknowledged = false;
    payload_path = NULL;
    payload_str = arg!=NULL ? cJSON_Print(arg) : NULL;
    payload_buff_length = payload_str!=NULL ? (strlen(payload_str) + 1) : 0;

    if( channel_request(rid, task, payload_str, payload_buff_length, log_ctx, conn, &payload_path) )
    {
        pshm->state = CHANNEL_REQUEST;
        strcpy(pshm->rid, rid);
        strcpy(pshm->task, task);
        pshm->payload_buff_length = payload_buff_length;

        pthread_cond_signal(&pshm->cond_proxy_wakeup);
        pthread_mutex_unlock(&pshm->mtx);
        usleep(1000);

        busy = true;
        died = false;
        while( busy && ttl-- > 0 ) {

            if( pthread_mutex_lock(&pshm->mtx)==EOWNERDEAD ) {
                proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                died = true;
                break;
            }

            if(pshm->state == CHANNEL_REQUEST) {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 10; //wait 10 seconds from now
                if( pthread_cond_timedwait( &pshm->cond_dispatcher_wakeup, &pshm->mtx, &ts )==EOWNERDEAD ) {
                    pthread_mutex_consistent(&pshm->mtx);
                    proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                    died = TRUE;
                    break;
                }
            }

            if(pshm->state != CHANNEL_REQUEST) {
                if(pshm->state == CHANNEL_ACKNOWLEDGED) {
                    acknowledged = true;
                    reply_status(conn, rid, SERVICE_STATUS_REQUEST_ACKNOWLEDGED);
                }
                else if(pshm->state == CHANNEL_INVALID_TASK)
                    reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_INVALID_TASK);
                else if(pshm->state == CHANNEL_INVALID_PAYLOAD)
                    reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_INVALID_PAYLOAD);
                else
                    reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_INVALID_STATE);
                busy = false;
                pshm->state = CHANNEL_DONE;
                pthread_cond_signal(&pshm->cond_proxy_wakeup);
            }

            pthread_mutex_unlock(&pshm->mtx);
            usleep(1000);
        }

        if(died) {
            gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent", sawang_name);
            reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_DIED);
        } else if( busy) {
            gonggo_log(log_ctx, "ERROR", "sawang %s task %s channel request is not responded",
                sawang_name, task);
            reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_BUSY);
        }

        if( payload_path != NULL ) {
            shm_unlink(payload_path);
            free(payload_path);
        }
    }

    if(payload_str!=NULL)
        free(payload_str);

    return acknowledged;
}

static bool channel_request(const char *rid, const char *task,
    const char *payload_str, int payload_buff_length,
    const LogContext *log_ctx, struct mg_connection *conn,
    char **payload_path) {

    bool status;
    int fd;
    char *shm_buff;
    char buff[GONGGOLOGBUFLEN];

    *payload_path = NULL;
    status = true;

    if( payload_str != NULL) {
        *payload_path = (char*)malloc(strlen(rid)+2);
        sprintf(*payload_path, "/%s", rid);

        status = false;
        fd = -1;
        do {
            fd = shm_open(*payload_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
            if( fd == -1 ) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR", "shm %s creation failed, %s", *payload_path, buff);
                reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_OPEN_FAILED);
                break;
            }
            ftruncate(fd, payload_buff_length);

            shm_buff = (char*)mmap(NULL, payload_buff_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
            if( shm_buff == MAP_FAILED ) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log(log_ctx, "ERROR", "shm %s map failed, %s", *payload_path, buff);
                reply_status(conn, rid, SERVICE_STATUS_PROXY_CHANNEL_ACCESS_FAILED);
                break;
            }

            strcpy(shm_buff, payload_str);
            munmap(shm_buff, payload_buff_length);
            status = true;
        } while(false);

        if( fd > -1 )
            close(fd);

        if(!status && *payload_path != NULL) {
            free(*payload_path);
            *payload_path = NULL;
        }
    }

    return status;
}

static bool channel_dead_request_conversation(const char *sawang_name, const LogContext *log_ctx, GSList *rid_list,
    ChannelSegmentData *pshm, GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table)
{
    char *payload_path;
    unsigned int payload_buff_length, ttl;
    bool acknowledged, busy, died;
    struct timespec ts;

    ttl = 5;
    acknowledged = false;
    payload_path = NULL;
    payload_buff_length = 0;

    if( channel_dead_request_payload(sawang_name, log_ctx, rid_list, &payload_path, &payload_buff_length) )
    {
        pshm->state = CHANNEL_DEAD_REQUEST;
        strcpy(pshm->rid, "");
        strcpy(pshm->task, "");
        pshm->payload_buff_length = payload_buff_length;

        pthread_cond_signal(&pshm->cond_proxy_wakeup);
        pthread_mutex_unlock(&pshm->mtx);
        usleep(1000);

        busy = true;
        died = false;
        while( busy && ttl-- > 0 ) {

            if( pthread_mutex_lock(&pshm->mtx)==EOWNERDEAD ) {
                proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                died = true;
                break;
            }

            if(pshm->state == CHANNEL_DEAD_REQUEST) {
                clock_gettime(CLOCK_REALTIME, &ts);
                ts.tv_sec += 10; //wait 10 seconds from now
                if( pthread_cond_timedwait( &pshm->cond_dispatcher_wakeup, &pshm->mtx, &ts )==EOWNERDEAD ) {
                    pthread_mutex_consistent(&pshm->mtx);
                    proxy_remove_with_lock(log_ctx, mtx_thread_table, proxy_thread_table, sawang_name);
                    died = TRUE;
                    break;
                }
            }

            if(pshm->state != CHANNEL_DEAD_REQUEST) {
                acknowledged = pshm->state == CHANNEL_ACKNOWLEDGED;
                busy = false;
                pshm->state = CHANNEL_DONE;
                pthread_cond_signal(&pshm->cond_proxy_wakeup);
            }

            pthread_mutex_unlock(&pshm->mtx);
            usleep(1000);
        }

        if(died)
            gonggo_log(log_ctx, "ERROR", "sawang %s left channel mutex inconsistent for request removal", sawang_name);
        else if( busy)
            gonggo_log(log_ctx, "ERROR", "sawang %s channel request removal is not responded", sawang_name);

        if( payload_path != NULL ) {
            shm_unlink(payload_path);
            free(payload_path);
        }
    }

    return acknowledged;
}

static bool channel_dead_request_payload(const char *sawang_name, const LogContext *log_ctx, GSList *rid_list,
    char **payload_path, unsigned int *payload_buff_length)
{
    guint len, i;
    int fd;
    char buff[GONGGOLOGBUFLEN];
    char *shm_buff, *run;
    bool status;

    len = g_slist_length(rid_list);
    *payload_buff_length = (len * (UUIDBUFLEN-1)) + 1/*zero terminator*/;

    *payload_path = (char*)malloc(strlen(sawang_name) + strlen(REMOVE_REQUEST_SUFFIX) + 2/*backslash and zero terminator*/);
    sprintf(*payload_path, "/%s%s", sawang_name, REMOVE_REQUEST_SUFFIX);

    status = false;
    fd = -1;
    do {
        fd = shm_open(*payload_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if( fd == -1 ) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "shm %s creation for request removal is failed, %s", *payload_path, buff);
            break;
        }
        ftruncate(fd, *payload_buff_length);

        shm_buff = (char*)mmap(NULL, *payload_buff_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if( shm_buff == MAP_FAILED ) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "shm %s map for request removal is failed, %s", *payload_path, buff);
            break;
        }

        run = shm_buff;
        for(i=0; i<len; i++) {
            strncpy( run, (const char*)g_slist_nth_data(rid_list, i), UUIDBUFLEN-1);
            run += (UUIDBUFLEN-1);
        }
        *run=0;//string zero terminator

        munmap(shm_buff, *payload_buff_length);
        status = true;
    } while(false);

    if( fd > -1 )
        close(fd);

    if(!status && *payload_path != NULL) {
        free(*payload_path);
        *payload_path = NULL;
    }

    return status;
}
