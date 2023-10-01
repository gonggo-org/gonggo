#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <stdio.h>
#include <errno.h>

#include "cJSON.h"

#include "proxysubscribe.h"
#include "proxythreadtable.h"
#include "requesttable.h"
#include "db.h"

#define SUBSCRIBE_SUFFIX "_subscribe"

static void subscribe_conversation(const char *subscribe_path,
    ProxySubscribeSegmentData *pshm, const LogContext *log_ctx,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table, const DbContext *db_ctx);
static void subscribe_reply(const cJSON *root, const char *rid,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table, bool remove_request,
    const LogContext *log_ctx, const DbContext *db_ctx);

const char* proxy_segment_label(enum ProxySegment segment) {
    const char *label;
    switch(segment) {
        case CHANNEL:
            label = "Channel";
            break;
        case SUBSCRIBE:
            label = "Subscribe";
            break;
        case HEARTBEAT:
            label = "Heartbeat";
            break;
        default:
            label = "Unknown";
            break;
    }
    return label;
}

ProxySubscribeThreadData* proxy_subscribe_thread_data_create(
    const LogContext *log_ctx, GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    const char *sawang_name, const DbContext *db_ctx,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table)
{
    ProxySubscribeThreadData* proxy_thread_data;
    char *subscribe_path;
    int fd;

    proxy_thread_data = (ProxySubscribeThreadData*)malloc(sizeof(ProxySubscribeThreadData));
    proxy_thread_data->log_ctx = log_ctx;
    proxy_thread_data->request_table = request_table; //map rid to client conn
    proxy_thread_data->mtx_request_table = mtx_request_table;
    strcpy(proxy_thread_data->sawang_name, sawang_name);
    proxy_thread_data->segment = NULL;
    proxy_thread_data->db_ctx = db_ctx;
    proxy_thread_data->proxy_thread_table = proxy_thread_table;
    proxy_thread_data->mtx_thread_table = mtx_thread_table;
    proxy_thread_data->stop = false;
    proxy_thread_data->die = false;

    subscribe_path = proxy_subscribe_path( sawang_name );
    fd = shm_open(subscribe_path, O_RDWR, S_IRUSR | S_IWUSR);
    proxy_thread_data->segment = (ProxySubscribeSegmentData*)mmap(NULL, sizeof(ProxySubscribeSegmentData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    free( subscribe_path );

    return proxy_thread_data;
}

void proxy_subscribe_thread_data_destroy(ProxySubscribeThreadData *data) {
    if( data->segment != NULL ) {
        munmap(data->segment, sizeof(ProxySubscribeSegmentData));
        data->segment = NULL;
    }
}

char* proxy_subscribe_path(const char *sawang_name) {
    char *subscribe_path;

    subscribe_path = (char*)malloc(strlen(sawang_name) + strlen(SUBSCRIBE_SUFFIX) + 2);
    sprintf(subscribe_path, "/%s%s", sawang_name, SUBSCRIBE_SUFFIX);
    return subscribe_path;
}

void* proxy_subscribe_thread(void *arg) {
    ProxySubscribeThreadData *p = (ProxySubscribeThreadData*)arg;
    struct timespec ts;
    char *subscribe_path;

    gonggo_log(p->log_ctx, "INFO", "proxy %s subscriber thread is started", p->sawang_name);

    while( !p->stop ) {
        if( pthread_mutex_lock( &p->segment->mtx )==EOWNERDEAD ) {
            gonggo_log(p->log_ctx, "ERROR", "sawang %s left subscribe mutex inconsistent", p->sawang_name);
            proxy_thread_stop_async(p->log_ctx, p->mtx_thread_table, p->proxy_thread_table, p->sawang_name);
            break;
        }

        if( p->segment->state != SUBSCRIBE_ANSWER ) {
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_sec += 5; //wait 5 seconds from now
            if( pthread_cond_timedwait( &p->segment->cond_dispatcher_wakeup, &p->segment->mtx, &ts )==EOWNERDEAD ) {
                pthread_mutex_consistent(&p->segment->mtx);
                gonggo_log(p->log_ctx, "ERROR", "sawang %s left subscribe mutex inconsistent", p->sawang_name);
                proxy_thread_stop_async(p->log_ctx, p->mtx_thread_table, p->proxy_thread_table, p->sawang_name);
                break;
            }
        }

        if( p->segment->state == SUBSCRIBE_ANSWER ) {
            subscribe_path = proxy_subscribe_path( p->sawang_name );
            subscribe_conversation(subscribe_path, p->segment, p->log_ctx, p->request_table, p->mtx_request_table, p->db_ctx);
            free( subscribe_path );
        } else {
            pthread_mutex_unlock( &p->segment->mtx );
            usleep(1000); //1000us = 1ms
        }
    }

    gonggo_log(p->log_ctx, "INFO", "proxy %s subscriber thread is stopped", p->sawang_name);
    p->die = true;
    pthread_exit(NULL);
}

static void subscribe_conversation(const char *subscribe_path,
    ProxySubscribeSegmentData *pshm, const LogContext *log_ctx,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    const DbContext *db_ctx)
{
    char *payload_path, buff[GONGGOLOGBUFLEN], *payload_str;
    int fd;
    cJSON *root, *root2, *item, *sub_item;
    int rid_count, i;
    const char *rid;

    payload_path = NULL;
    fd = -1;
    root = NULL;
    payload_str = NULL;

    do {
        if( pshm->payload_buff_length < 1 ) {
            gonggo_log(log_ctx, "ERROR", "subscriber path %s answer does not have payload",
                subscribe_path);
            pshm->state = SUBSCRIBE_FAILED;
            pthread_cond_signal(&pshm->cond_proxy_wakeup);
            break;
        }

        payload_path = (char*)malloc(strlen(pshm->aid)+2/*backslash and zero terminator*/);
        sprintf(payload_path, "/%s", pshm->aid);
        fd = shm_open(payload_path, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log(log_ctx, "ERROR", "open shared memory % of subscriber path %s failed, %s",
                payload_path, subscribe_path, buff);
            pshm->state = SUBSCRIBE_FAILED;
            pthread_cond_signal(&pshm->cond_proxy_wakeup);
            break;
        }

        payload_str = (char*)mmap(NULL, pshm->payload_buff_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        root = cJSON_Parse(payload_str);
        munmap(payload_str, pshm->payload_buff_length);

        item = cJSON_GetObjectItem(root, RIDKEY);
        if( item==NULL ) {
            gonggo_log(log_ctx, "ERROR", "subscriber path %s answer does not have rid",
                subscribe_path);
            pshm->state = SUBSCRIBE_FAILED;
            pthread_cond_signal(&pshm->cond_proxy_wakeup);
            break;
        }

        if( !cJSON_IsArray(item) && !cJSON_IsString(item) ) {
            gonggo_log(log_ctx, "ERROR", "subscriber path %s answer rid is not an array nor a string",
                subscribe_path);
            pshm->state = SUBSCRIBE_FAILED;
            pthread_cond_signal(&pshm->cond_proxy_wakeup);
            break;
        }

        if( cJSON_IsArray(item) ) {
            root2 = cJSON_Duplicate(root, true);
            rid_count = cJSON_GetArraySize(item);
            for(i=0; i<rid_count; i++) {
                sub_item = cJSON_GetArrayItem(item, i);
                rid = cJSON_GetStringValue(sub_item);
                cJSON_DeleteItemFromObject(root2, RIDKEY);
                cJSON_AddStringToObject(root2, RIDKEY, rid);
                subscribe_reply(root2, rid, request_table, mtx_request_table, pshm->remove_request, log_ctx, db_ctx);
            }
            cJSON_Delete(root2);
        } else if( cJSON_IsString(item) ) {
            rid = cJSON_GetStringValue(item);
            subscribe_reply(root, rid, request_table, mtx_request_table, pshm->remove_request, log_ctx, db_ctx);
        }

        pshm->state = SUBSCRIBE_DONE;
        pthread_cond_signal(&pshm->cond_proxy_wakeup);
    } while(false);

    pthread_mutex_unlock(&pshm->mtx);
    usleep(1000);

    if(payload_path!=NULL)
        free(payload_path);
    if(fd>-1)
        close(fd);
    if(root!=NULL)
        cJSON_Delete(root);
    return;
}

static void subscribe_reply(const cJSON *root, const char *rid,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table, bool remove_request,
    const LogContext *log_ctx, const DbContext *db_ctx)
{
    struct mg_connection* conn;
    char *s;

    s = cJSON_Print(root);
    db_respond_insert(db_ctx, log_ctx, rid, s);
    conn = request_table_conn_get(request_table, mtx_request_table, rid, true, remove_request);
    if(conn!=NULL)
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, s, strlen(s));
    free(s);

    return;
}
