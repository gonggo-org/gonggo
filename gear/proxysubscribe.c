#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include "log.h"
#include "proxysubscribe.h"
#include "cJSON.h"
#include "db.h"
#include "clientrequesttable.h"
#include "clientconnectiontable.h"
#include "clientproxynametable.h"
#include "clientservicetable.h"
#include "clientreply.h"
#include "util.h"

#define SUBSCRIBE_SUFFIX "_subscribe"

static char* proxy_subscribe_path(const char *proxy_name);
static enum ProxySubscribeState proxy_subscribe_exchange(const char *proxy_name, ProxySubscribeShm *shm, const char *subscribe_path);
static void proxy_subscribe_purge_rid(const char *rid);
static void proxy_subscribe_reply(const char *proxy_name, const char *rid, cJSON *headers, cJSON *payload, bool remove_request);    

ProxySubscribeShm *proxy_subscribe_shm_get(const char *proxy_name) {
    char *p;
    int fd;
    char buff[GONGGOLOGBUFLEN];
    ProxySubscribeShm *shm = NULL;
   
    p = NULL;
    fd = -1;
    do {
        p = proxy_subscribe_path(proxy_name);
        fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for SUBSCRIBE segment of proxy %s, %s", p, buff);
            break;
        }
        shm = (ProxySubscribeShm*)mmap(NULL, sizeof(ProxySubscribeShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(shm == MAP_FAILED) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for SUBSCRIBE segment of proxy %s, %s", p, buff);
            shm = NULL;
            break;
        }
    } while (false);

    if(fd>-1) {
        close(fd);
    }

    if(p!=NULL) {
        free(p);
    }

    return shm;
}

ProxySubscribeContext* proxy_subscribe_context_create(const char* proxy_name, ProxySubscribeShm *shm) 
{
    ProxySubscribeContext* ctx;

    ctx = (ProxySubscribeContext*)malloc(sizeof(ProxySubscribeContext));
    ctx->proxy_name = strdup(proxy_name);
    ctx->shm = shm;
    ctx->stop = false;
    return ctx;
}

void proxy_subscribe_context_destroy(ProxySubscribeContext* ctx) {
    char *path;
    if(ctx!=NULL) {
        if(ctx->shm!=NULL) {
            pthread_mutex_destroy(&ctx->shm->lock);

            gonggo_cond_reset(&ctx->shm->dispatcher_wakeup);
            pthread_cond_destroy(&ctx->shm->dispatcher_wakeup);

            gonggo_cond_reset(&ctx->shm->proxy_wakeup);
            pthread_cond_destroy(&ctx->shm->proxy_wakeup);

            munmap(ctx->shm, sizeof(ProxySubscribeShm));
            ctx->shm = NULL;
        }
        if(ctx->proxy_name!=NULL) {
            path = proxy_subscribe_path(ctx->proxy_name);
            shm_unlink(path);
            free(path);
            free(ctx->proxy_name); 
            ctx->proxy_name = NULL;
        }        
        free(ctx);
    }
}

void* proxy_subscribe(void *arg) {
    ProxySubscribeContext* ctx;
    char *subscribe_path;
    bool proxy_died;
    
    ctx = (ProxySubscribeContext*)arg;
    gonggo_log("INFO", "proxy %s subscribe thread is started", ctx->proxy_name);

    if( (proxy_died = pthread_mutex_lock(&ctx->shm->lock)==EOWNERDEAD) ){
        pthread_mutex_consistent(&ctx->shm->lock);
    }

    while(!proxy_died && !ctx->stop && ctx->shm->state!=SUBSCRIBE_TERMINATION) {
        if(pthread_cond_wait(&ctx->shm->dispatcher_wakeup, &ctx->shm->lock) == EOWNERDEAD) {
            pthread_mutex_consistent(&ctx->shm->lock);
            break;
        }
        if(!ctx->stop && ctx->shm->state == SUBSCRIBE_ANSWER) {            
            subscribe_path = proxy_subscribe_path(ctx->proxy_name);
            ctx->shm->state = proxy_subscribe_exchange(ctx->proxy_name, ctx->shm, subscribe_path);
                //return SUBSCRIBE_FAILED or SUBSCRIBE_DONE
            free(subscribe_path);
            pthread_cond_signal(&ctx->shm->proxy_wakeup);
        }   
    }

    pthread_mutex_unlock(&ctx->shm->lock);

    gonggo_log("INFO", "proxy %s subscribe thread is stopped", ctx->proxy_name);
    pthread_exit(NULL);
}

void proxy_subscribe_stop(ProxySubscribeContext* ctx) {
    if(pthread_mutex_lock(&ctx->shm->lock)==EOWNERDEAD) {
        pthread_mutex_consistent(&ctx->shm->lock);
    }
    ctx->stop = true;
    ctx->shm->state = SUBSCRIBE_TERMINATION;
    pthread_cond_signal(&ctx->shm->dispatcher_wakeup);
    pthread_mutex_unlock(&ctx->shm->lock);
}

static char* proxy_subscribe_path(const char *proxy_name) {
    char *subscribe_path;

    subscribe_path = (char*)malloc(strlen(proxy_name) + strlen(SUBSCRIBE_SUFFIX) + 2);
    sprintf(subscribe_path, "/%s%s", proxy_name, SUBSCRIBE_SUFFIX);
    return subscribe_path;
}

static enum ProxySubscribeState proxy_subscribe_exchange(const char *proxy_name,
    ProxySubscribeShm *shm, const char *subscribe_path) 
{
    char *payload_path, *payload_str; 
    char buff[GONGGOLOGBUFLEN];
    const char *unsubscribe_rid;
    int fd, i, count, service_status;
    cJSON *root, *rid_item, *sub_rid_item, *headers, *payload, *service_status_item, *item;
    enum ProxySubscribeState state;
    
    if(shm->payload_buff_length < 1) {
        gonggo_log("ERROR", "subscriber path %s answer does not have payload", subscribe_path);            
        return SUBSCRIBE_FAILED;
    }

    payload_path = NULL;
    root = NULL;
    state = SUBSCRIBE_FAILED;
    do {
        payload_path = (char*)malloc(strlen(shm->aid)+2/*backslash and zero terminator*/);
        sprintf(payload_path, "/%s", shm->aid);
        fd = shm_open(payload_path, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd == -1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "open shared memory % of subscriber path %s failed, %s", payload_path, subscribe_path, buff);
            break;
        }

        payload_str = (char*)mmap(NULL, shm->payload_buff_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        root = cJSON_Parse(payload_str);
        munmap(payload_str, shm->payload_buff_length);

        rid_item = cJSON_GetObjectItem(root, CLIENTREPLY_SERVICE_RID_KEY);
        if(rid_item==NULL) {
            gonggo_log("ERROR", "subscriber path %s answer does not have rid", subscribe_path);
            break;
        }

        if( !cJSON_IsArray(rid_item) && !cJSON_IsString(rid_item) ) {
            gonggo_log("ERROR", "subscriber path %s answer rid is not an array nor a string", subscribe_path);
            break;
        }      

        headers = cJSON_GetObjectItem(root, CLIENTREPLY_HEADERS_KEY);//mandatory
        if(headers==NULL) {
            gonggo_log("ERROR", "subscriber path %s answer does not have %s", subscribe_path, CLIENTREPLY_HEADERS_KEY);
            break;
        }

        payload = cJSON_GetObjectItem(root, CLIENTREPLY_PAYLOAD_KEY);//optional

        service_status_item = payload!=NULL ? cJSON_GetObjectItem(headers, CLIENTREPLY_SERVICE_STATUS_KEY) : NULL;
        if(service_status_item!=NULL) {
            service_status = (int)cJSON_GetNumberValue(service_status_item);
            if(service_status==PROXYSERVICESTATUS_MULTIRESPOND_CLEAR_SUCCESS) {
                item = cJSON_GetObjectItem(payload, SERVICE_RID_KEY);
                unsubscribe_rid = item!=NULL ? cJSON_GetStringValue(item) : NULL;
                if(unsubscribe_rid!=NULL) {
                    proxy_subscribe_purge_rid(unsubscribe_rid);
                }
                payload = NULL;        
            }
        }

        if( cJSON_IsArray(rid_item) ) {
            count = cJSON_GetArraySize(rid_item);
            for(i=0; i<count; i++) {
                sub_rid_item = cJSON_GetArrayItem(rid_item, i);
                proxy_subscribe_reply(proxy_name, cJSON_GetStringValue(sub_rid_item), 
                    cJSON_Duplicate(headers, true), 
                    payload!=NULL ? cJSON_Duplicate(payload, true) : NULL,
                    shm->remove_request);
            }
        } else if( cJSON_IsString(rid_item) ) {
            proxy_subscribe_reply(proxy_name, cJSON_GetStringValue(rid_item), 
                cJSON_Duplicate(headers, true), 
                payload!=NULL ? cJSON_Duplicate(payload, true) : NULL,                
                shm->remove_request);                
        }        

        state = SUBSCRIBE_DONE;
    } while(false);

    if(payload_path!=NULL) {
        free(payload_path);
    }

    if(fd>-1) {
        close(fd);
    }

    if(root!=NULL) {
        cJSON_Delete(root);
    }

    return state;
}

static void proxy_subscribe_purge_rid(const char *rid) {
    struct mg_connection* conn;

    conn = client_request_table_get_conn(rid, false/*update time*/, true);
    if(conn!=NULL) {
        client_connection_table_drop(conn, rid);
    }
    client_proxyname_table_drop_all(rid);//search in all proxy_names
    client_service_table_remove(rid);
}

static void proxy_subscribe_reply(    
    const char *proxy_name, const char *rid, 
    cJSON *headers, cJSON *payload, bool remove_request)
{
    struct mg_connection* conn;    

    conn = client_request_table_get_conn(rid, true/*update time*/, remove_request);
    if(remove_request) {
        if(conn!=NULL) {
            client_connection_table_drop(conn, rid);
        }
        client_proxyname_table_drop(proxy_name, rid);
        client_service_table_remove(rid);
    }
    client_service_reply(conn, rid, headers, payload, true);
}