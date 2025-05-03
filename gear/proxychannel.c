#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>

#include "log.h"
#include "proxychannel.h"
#include "clientproxynametable.h"
#include "clientservicetable.h"
#include "proxyterminatearray.h"
#include "util.h"
#include "gonggouuid.h"

#define CHANNEL_SUFFIX "_channel"

static char* proxy_channel_path(const char *proxy_name);
static bool proxy_channel_exchange(ProxyChannelContext* ctx);
static bool proxy_channel_idle_wait(ProxyChannelContext* ctx);
static size_t proxy_channel_create_request(const char *payload_path, const char *proxy_name, const char *service_name_payload);

ProxyChannelShm *proxy_channel_shm_get(const char *proxy_name) {
    char *p;
    int fd;
    char buff[GONGGOLOGBUFLEN];
    ProxyChannelShm *shm = NULL;
   
    p = NULL;
    fd = -1;
    do {
        p = proxy_channel_path(proxy_name);
        fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for CHANNEL segment of proxy %s, %s", p, buff);
            break;
        }
        shm = (ProxyChannelShm*)mmap(NULL, sizeof(ProxyChannelShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(shm == MAP_FAILED) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for CHANNEL segment of proxy %s, %s", p, buff);
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

ProxyChannelContext* proxy_channel_context_create(const char* proxy_name, ProxyChannelShm *shm) 
{
    ProxyChannelContext* ctx;
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    ctx = (ProxyChannelContext*)malloc(sizeof(ProxyChannelContext));
    ctx->proxy_name = strdup(proxy_name);
    ctx->shm = shm;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_PRIVATE);
    pthread_mutexattr_setrobust(&mutexattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_NORMAL);
    pthread_mutex_init(&ctx->lock, &mutexattr);
    pthread_mutexattr_destroy(&mutexattr);//mutexattr is no longer needed

    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_PRIVATE);
    pthread_cond_init(&ctx->wakeup, &condattr);
    pthread_condattr_destroy(&condattr);//condattr is no longer needed

    ctx->stop = false;

    return ctx;

}

void proxy_channel_context_destroy(ProxyChannelContext* ctx) {
    char *path;

    if(ctx!=NULL) {
        pthread_mutex_destroy(&ctx->lock);
        pthread_cond_destroy(&ctx->wakeup);
        if(ctx->shm!=NULL) {
            pthread_mutex_destroy(&ctx->shm->lock);

            gonggo_cond_reset(&ctx->shm->dispatcher_wakeup);
            pthread_cond_destroy(&ctx->shm->dispatcher_wakeup);

            gonggo_cond_reset(&ctx->shm->proxy_wakeup);
            pthread_cond_destroy(&ctx->shm->proxy_wakeup);

            gonggo_cond_reset(&ctx->shm->idle);
            pthread_cond_destroy(&ctx->shm->idle);

            munmap(ctx->shm, sizeof(ProxyChannelShm));
            ctx->shm=NULL;
        }        
        if(ctx->proxy_name!=NULL) {
            path = proxy_channel_path(ctx->proxy_name);        
            shm_unlink(path);            
            free(path);
            free(ctx->proxy_name);
            ctx->proxy_name = NULL;
        }
        free(ctx);
    }
}

void* proxy_channel(void *arg) {
    ProxyChannelContext* ctx;

    ctx = (ProxyChannelContext*)arg;
    gonggo_log("INFO", "proxy %s channel thread is started", ctx->proxy_name);

    pthread_mutex_lock(&ctx->lock);
    while(!ctx->stop) {
        if(proxy_terminate_array_exists(ctx->proxy_name)){//in case proxy activation timeout
            if(proxy_channel_idle_wait(ctx)){
                ctx->shm->state = CHANNEL_STOP_REQUEST;
                pthread_cond_signal(&ctx->shm->proxy_wakeup);
                pthread_mutex_unlock(&ctx->shm->lock);
            }
            proxy_terminate_array_drop(ctx->proxy_name);
            break;
        } else {                                 
            if(!proxy_channel_exchange(ctx)){
                break;
            }
        }
        pthread_cond_wait(&ctx->wakeup, &ctx->lock);        
    }
    pthread_mutex_unlock(&ctx->lock);

    gonggo_log("INFO", "proxy %s channel thread is stopped", ctx->proxy_name);
    pthread_exit(NULL);
}

void proxy_channel_stop(ProxyChannelContext* ctx) {
    if(pthread_mutex_lock(&ctx->shm->lock)==EOWNERDEAD) {
        pthread_mutex_consistent(&ctx->shm->lock);  
    }
    ctx->shm->state = CHANNEL_TERMINATION;
    pthread_cond_signal(&ctx->shm->idle);
    pthread_cond_signal(&ctx->shm->dispatcher_wakeup);
    pthread_mutex_unlock(&ctx->shm->lock);
    
    pthread_mutex_lock(&ctx->lock);    
    ctx->stop = true;
    pthread_cond_signal(&ctx->wakeup);
    pthread_mutex_unlock(&ctx->lock);
}

void proxy_channel_rest(ProxyChannelContext* ctx, const char *endpoint, const cJSON *payload, RestRespond* respond) {
    char rid[UUIDBUFLEN], *payload_path, *service_name_payload, *respond_path, buff[GONGGOLOGBUFLEN], *answer_str;
    int fd;
    cJSON *answer_json;

    respond->code = 0;
    respond->err = NULL;
    respond->json = NULL;
    
    gonggo_log("INFO", "gonggo wait for proxy_channel_idle_wait");    
    if(!proxy_channel_idle_wait(ctx)){
        respond->code = 500;
        respond->err = strdup("proxy is out of service");
        return;
    }

    gonggo_log("INFO", "gonggo gonggo_uuid_generate");   
    gonggo_uuid_generate(rid);

    payload_path = (char*)malloc(strlen(rid)+2);
    sprintf(payload_path, "/%s", rid);      
    service_name_payload = client_service_table_service_payload_create(endpoint, payload);

    do {
        ctx->shm->payload_buff_length = proxy_channel_create_request(payload_path, ctx->proxy_name, service_name_payload);
        if(ctx->shm->payload_buff_length<1) {
            respond->code = 500;
            respond->err = strdup("proxy is out of order");
            break;
        }

        strcpy(ctx->shm->rid, rid);
        ctx->shm->state = CHANNEL_REST;        
        pthread_cond_signal(&ctx->shm->proxy_wakeup);

        gonggo_log("INFO", "gonggo pthread_cond_wait dispatcher_wakeup)");
        if(pthread_cond_wait(&ctx->shm->dispatcher_wakeup, &ctx->shm->lock)==EOWNERDEAD) {
            gonggo_log("INFO", "gonggo pthread_cond_wait wakeup EOWNERDEAD)");
            pthread_mutex_consistent(&ctx->shm->lock);
            respond->code = 500;
            respond->err = strdup("proxy is out of order");
        } else if(ctx->shm->state==CHANNEL_REST_RESPOND) {
            gonggo_log("INFO", "gonggo pthread_cond_wait wakeup CHANNEL_REST_RESPOND)");
            respond_path = (char*)malloc(strlen(ctx->shm->aid)+2/*backslash and zero terminator*/);
            sprintf(respond_path, "/%s", ctx->shm->aid);
            fd = shm_open(respond_path, O_RDWR, S_IRUSR | S_IWUSR);
            if(fd == -1) {
                strerror_r(errno, buff, GONGGOLOGBUFLEN);
                gonggo_log("ERROR", "open shared memory % of REST respond path is failed, %s", respond_path, buff);
                respond->code = 500;
                respond->err = strdup("proxy is out of order");
            } else {
                answer_str = (char*)mmap(NULL, ctx->shm->answer_buff_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
                answer_json = cJSON_Parse(answer_str);
                munmap(answer_str, ctx->shm->answer_buff_length);
            
            ////setup respond:BEGIN
                respond->code = cJSON_GetNumberValue( cJSON_GetObjectItem(answer_json, "code") );
                if(cJSON_HasObjectItem(answer_json, "err")){
                    respond->err = strdup(cJSON_GetStringValue(cJSON_GetObjectItem(answer_json, "err")));
                }
                if(cJSON_HasObjectItem(answer_json, "json")){
                    respond->json = cJSON_PrintUnformatted(cJSON_GetObjectItem(answer_json, "json"));
                }
            ////setup respond:END

                cJSON_Delete(answer_json);
            }           
            free(respond_path);
            ctx->shm->state = CHANNEL_DONE;
            pthread_cond_signal(&ctx->shm->proxy_wakeup);
        }
    } while(false);

    pthread_mutex_unlock(&ctx->shm->lock);
    free(service_name_payload);
    free(payload_path);
}

void proxy_channel_rest_respond_destroy(RestRespond* respond) {
    if(respond->err != NULL) {
        free(respond->err);
        respond->err = NULL;
    }
    if(respond->json != NULL) {
        free(respond->json);
        respond->json = NULL;
    }
}

static bool proxy_channel_exchange(ProxyChannelContext* ctx) {
    bool alive = true;
    GPtrArray* request_uuid_arr;
    guint i, len;
    char *request_uuid;
    char *payload_path, *service_name_payload;

    request_uuid_arr = client_proxyname_table_request_dup(ctx->proxy_name, true/*not_sent_only*/);
    len = request_uuid_arr!=NULL ? request_uuid_arr->len : 0;
    for(i=0; alive && i<len; i++) {
        request_uuid = g_ptr_array_index(request_uuid_arr, i);
        service_name_payload = client_service_table_dup(request_uuid);

        if(service_name_payload!=NULL) {
        ////channel data exchange:BEGIN
            payload_path = NULL;
            do {
                if(!proxy_channel_idle_wait(ctx)){
                    alive = false;
                    break;
                }

                payload_path = (char*)malloc(strlen(request_uuid)+2);
                sprintf(payload_path, "/%s", request_uuid);
                ctx->shm->payload_buff_length = proxy_channel_create_request(payload_path, ctx->proxy_name,
                    service_name_payload);
                if(ctx->shm->payload_buff_length<1) {
                    pthread_mutex_unlock(&ctx->shm->lock);
                    break;
                }
                strcpy(ctx->shm->rid, request_uuid);
                ctx->shm->state = CHANNEL_REQUEST;
                pthread_cond_signal(&ctx->shm->proxy_wakeup);

                if(pthread_cond_wait(&ctx->shm->dispatcher_wakeup, &ctx->shm->lock)==EOWNERDEAD) {
                    pthread_mutex_consistent(&ctx->shm->lock);
                    gonggo_log("ERROR", "proxy %s left channel exchange mutex inconsistent", ctx->proxy_name);
                    alive = false;
                } else if(ctx->shm->state==CHANNEL_TERMINATION) {
                    gonggo_log("INFO", "proxy %s channel exchange termination is detected", ctx->proxy_name);
                    alive = false;
                } else if(ctx->shm->state == CHANNEL_ACKNOWLEDGED || ctx->shm->state == CHANNEL_FAILS) {
                    if(ctx->shm->state == CHANNEL_ACKNOWLEDGED) {
                        gonggo_log("INFO", "proxy %s acknowledges the request", ctx->proxy_name);
                    } else if(ctx->shm->state == CHANNEL_FAILS) {
                        gonggo_log("INFO", "proxy %s fails to acknowledge the request", ctx->proxy_name);
                    }
                    ctx->shm->state = CHANNEL_DONE;
                    pthread_cond_signal(&ctx->shm->proxy_wakeup);
                    client_proxyname_table_request_sent(ctx->proxy_name, request_uuid);
                }

                pthread_mutex_unlock(&ctx->shm->lock);
                shm_unlink(payload_path);
            } while(false);

            if(payload_path!=NULL) {
                free(payload_path);
            }                
        ////channel data exchange:END

            free(service_name_payload);
        }        
        free(request_uuid);
    }

    if(request_uuid_arr!=NULL) {
        g_ptr_array_free(request_uuid_arr, false);
    }

    return alive;
}

static bool proxy_channel_idle_wait(ProxyChannelContext* ctx) {
    if(pthread_mutex_lock(&ctx->shm->lock)==EOWNERDEAD) {
        pthread_mutex_consistent(&ctx->shm->lock);
        gonggo_log("ERROR", "proxy %s left channel mutex inconsistent while waiting idle status", ctx->proxy_name);
        return false;
    }
    if(ctx->shm->state==CHANNEL_TERMINATION) {
        gonggo_log("ERROR", "proxy %s channel is in termination state after lock on waiting idle status", ctx->proxy_name);
        return false;
    }
    if(ctx->shm->state != CHANNEL_IDLE) {
        if(pthread_cond_wait(&ctx->shm->idle, &ctx->shm->lock)==EOWNERDEAD){
            pthread_mutex_consistent(&ctx->shm->lock);
            gonggo_log("ERROR", "proxy %s left mutex inconsistent waiting idle status", ctx->proxy_name);
            return false;
        }
        if(ctx->shm->state==CHANNEL_TERMINATION) {
            gonggo_log("ERROR", "proxy %s channel is in termination state on waiting idle status", ctx->proxy_name);
            return false;
        }        
    }
    return true; 
}

static char* proxy_channel_path(const char *proxy_name) {
    char *channel_path;

    channel_path = (char*)malloc(strlen(proxy_name) + strlen(CHANNEL_SUFFIX) + 2);
    sprintf(channel_path, "/%s%s", proxy_name, CHANNEL_SUFFIX);
    return channel_path;
}

static size_t proxy_channel_create_request(const char *payload_path, const char *proxy_name, const char *service_name_payload)
{
    int fd = -1;
    char buff[GONGGOLOGBUFLEN], *shm_buff;
    size_t buff_len;

    do {
        fd = shm_open(payload_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "proxy %s channel shared memory creation failed, %s", proxy_name, buff);
            buff_len = 0;
            break;
        }
        buff_len = strlen(service_name_payload) + 1;//zero terminated
        ftruncate(fd, buff_len);

        shm_buff = (char*)mmap(NULL, buff_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(shm_buff==MAP_FAILED) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "proxy %s channel shared memory map failed, %s", proxy_name, buff);
            shm_unlink(payload_path);
            buff_len = 0;
            break;
        }

        strcpy(shm_buff, service_name_payload);    
        munmap(shm_buff, buff_len);
    } while(false);

    if(fd!=-1) {
        close(fd);
    }

    return buff_len;
}