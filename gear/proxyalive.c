#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <errno.h>
#include <stdbool.h>

#include "log.h"
#include "proxy.h"
#include "clientreply.h"
#include "proxyterminator.h"

#define HEARTBEAT_SUFFIX "_alive"

static char* proxy_alive_path(const char *proxy_name);

ProxyAliveMutexShm *proxy_alive_shm_get(const char *proxy_name) {
    char *p;
    int fd;
    char buff[GONGGOLOGBUFLEN];
    ProxyAliveMutexShm *shm = NULL;
   
    p = NULL;
    fd = -1;
    do {
        p = proxy_alive_path(proxy_name);
        fd = shm_open(p, O_RDWR, S_IRUSR | S_IWUSR);
        if(fd==-1) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for ALIVE segment of proxy %s, %s", p, buff);
            break;
        }
        shm = (ProxyAliveMutexShm*)mmap(NULL, sizeof(ProxyAliveMutexShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
        if(shm == MAP_FAILED) {
            strerror_r(errno, buff, GONGGOLOGBUFLEN);
            gonggo_log("ERROR", "shared memory access failed for ALIVE segment of proxy %s, %s", p, buff);
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

ProxyAliveContext* proxy_alive_context_create(const char* proxy_name, pid_t pid, ProxyAliveMutexShm *shm) {
    ProxyAliveContext* ctx;

    ctx = (ProxyAliveContext*)malloc(sizeof(ProxyAliveContext));
    ctx->proxy_name = strdup(proxy_name);
    ctx->pid = pid;
    ctx->dead = false;
    ctx->shm = shm;
    return ctx;
}

void proxy_alive_context_destroy(ProxyAliveContext* ctx) {
    char *path;
    if(ctx!=NULL) {
        if(ctx->shm!=NULL) {
            munmap(ctx->shm, sizeof(ProxyAliveMutexShm));
            ctx->shm = NULL;
        }
        if(ctx->proxy_name!=NULL) {
            path = proxy_alive_path(ctx->proxy_name);
            shm_unlink(path);
            free(path);
            free(ctx->proxy_name);
            ctx->proxy_name = NULL;
        }        
        free(ctx);
    }
}

void* proxy_alive(void *arg) {
    ProxyAliveContext* ctx;
    int status;

    ctx = (ProxyAliveContext*)arg;

    gonggo_log("INFO", "proxy %s alive thread is started", ctx->proxy_name);

    while(!ctx->dead) {
        gonggo_log("INFO", "proxy %s alive thread waits for mutex", ctx->proxy_name);
        status = pthread_mutex_lock(&ctx->shm->lock);
        gonggo_log("INFO", "proxy %s alive thread mutex is acquired", ctx->proxy_name);
        if(status==EOWNERDEAD) {
            pthread_mutex_consistent(&ctx->shm->lock);
        }
        ctx->dead = status==EOWNERDEAD || (status==0 && !ctx->shm->alive);
        if(ctx->dead) {
            gonggo_log("INFO", "proxy %s alive thread proxy_alive_dead starts", ctx->proxy_name);
            proxy_thread_terminating_set(ctx->proxy_name);//should be removed in void* proxy_terminator(void *arg)
            proxy_terminator_awake(ctx->proxy_name);
            client_proxy_alive_notification(ctx->proxy_name, false);
            gonggo_log("INFO", "proxy %s alive thread proxy_alive_dead done", ctx->proxy_name);
        }
        pthread_mutex_unlock(&ctx->shm->lock);
        if(status==0 && !ctx->dead) {
            usleep(1000);
        }
    }

    gonggo_log("INFO", "proxy %s alive thread is stopped", ctx->proxy_name);

    pthread_exit(NULL);
}

void proxy_alive_stop(ProxyAliveContext* ctx) {
    gonggo_log("INFO", "proxy_alive_stop %ld proxy_alive_dead %s", ctx->pid, ctx->dead ? "true" : "false");
    if(!ctx->dead) {
        kill(ctx->pid, SIGTERM);
    }
}

static char* proxy_alive_path(const char *proxy_name) {
    char *heartbeat_path;

    heartbeat_path = (char*)malloc(strlen(proxy_name) + strlen(HEARTBEAT_SUFFIX) + 2);
    sprintf(heartbeat_path, "/%s%s", proxy_name, HEARTBEAT_SUFFIX);
    return heartbeat_path;   
}
/*
static void proxy_alive_cleanup(void *arg) {
    ProxyAliveContext* ctx;

    ctx = (ProxyAliveContext*)arg;
    gonggo_log("INFO", "proxy %s alive cleanup", ctx->proxy_name);
    pthread_mutex_unlock(&ctx->shm->lock);
}*/