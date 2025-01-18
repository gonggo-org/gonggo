#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <unistd.h>

#include "log.h"
#include "globaldata.h"
#include "proxysubscribethreadtable.h"
#include "proxychannelthreadtable.h"
#include "proxyalivethreadtable.h"
#include "clientproxynametable.h"
#include "clientrequesttable.h"
#include "clientreply.h"
#include "util.h"
#include "clientconnectiontable.h"

#include "proxyalive.h"
#include "proxyterminatearray.h"

#define PROXY_ACTIVATION_RESPOND_TIMEOUT_SECOND 10

//property
static char *proxy_activator_gonggo_path = NULL;
static volatile bool proxy_activator_started = false;
static bool proxy_activator_end = false;

enum ProxyActivationRespondState {
    ACTIVATIONSTATE_DISPATCHER_FAILED = 1,
    ACTIVATIONSTATE_DISPATCHER_SUCCESS = 2,
    ACTIVATIONSTATE_PROXY_TIMEOUT = 3,  //dispatcher success but proxy respond is timed-out 
    ACTIVATIONSTATE_PROXY_DEAD = 4,
    ACTIVATIONSTATE_TERMINATED = 99
};
static ProxyActivationShm *proxy_activator_shm = NULL;    

//function
static bool proxy_activator_shm_create(const char *gonggo_path);
static void proxy_activator_shm_idle(void);
static enum ProxyActivationRespondState proxy_activation(
    ProxyActivationShm *shm,
    pthread_attr_t *thread_attr, pthread_t *t_subscribe_thread, pthread_t *t_channel_thread, pthread_t *t_alive_thread,
    ProxySubscribeContext** subscribe_ctx, ProxyChannelContext** channel_ctx, ProxyAliveContext** alive_ctx);

bool proxy_activator_context_init(void)
{
    asprintf(&proxy_activator_gonggo_path, "/%s", gonggo_name);
    if(!proxy_activator_shm_create(proxy_activator_gonggo_path)){
        free(proxy_activator_gonggo_path);
        proxy_activator_gonggo_path = NULL;
        return false;
    }
    return true;
}

void proxy_activator_context_destroy(void) {
    if(proxy_activator_shm!=NULL) {
        pthread_mutex_destroy(&proxy_activator_shm->lock);

        gonggo_cond_reset(&proxy_activator_shm->dispatcher_wakeup);
        pthread_cond_destroy(&proxy_activator_shm->dispatcher_wakeup);

        gonggo_cond_reset(&proxy_activator_shm->proxy_wakeup);
        pthread_cond_destroy(&proxy_activator_shm->proxy_wakeup);

        munmap(proxy_activator_shm, sizeof(ProxyActivationShm));
        proxy_activator_shm = NULL;   
    }
    if(proxy_activator_gonggo_path!=NULL) {
        shm_unlink(proxy_activator_gonggo_path);
        free(proxy_activator_gonggo_path);
        proxy_activator_gonggo_path = NULL;
    }
}

void* proxy_activator(void *arg) {
    bool proxy_dead;
    pthread_attr_t thread_attr;
    pthread_t t_subscribe_thread, t_channel_thread, t_alive_thread;
    ProxySubscribeContext* subscribe_ctx;
    ProxyChannelContext* channel_ctx;
    ProxyAliveContext* alive_ctx;
    enum ProxyActivationRespondState activation_state;
    
    gonggo_log("INFO", "proxy activation listener thread is started");            

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);

    if(pthread_mutex_lock(&proxy_activator_shm->lock) == EOWNERDEAD) {
        pthread_mutex_consistent(&proxy_activator_shm->lock);//resurrection
    }

    proxy_activator_started = true;

    while(!proxy_activator_end) {
        proxy_activator_shm_idle();//set state to ACTIVATION_IDLE
        pthread_cond_signal(&proxy_activator_shm->proxy_wakeup); //let proxy knows

        proxy_dead = pthread_cond_wait(&proxy_activator_shm->dispatcher_wakeup, &proxy_activator_shm->lock)==EOWNERDEAD;
        if(proxy_dead) {
            pthread_mutex_consistent(&proxy_activator_shm->lock);//resurrection
        }
        if(proxy_activator_shm->state==ACTIVATION_TERMINATION) {
            break;
        }
        if( !proxy_activator_end 
            && !proxy_dead 
            && strlen(proxy_activator_shm->proxy_name) > 0
            && (proxy_activator_shm->state == ACTIVATION_REQUEST)
        ) {
            gonggo_log("INFO", "proxy %s activation is detected", proxy_activator_shm->proxy_name);
            activation_state = proxy_activation(proxy_activator_shm,
                &thread_attr, &t_subscribe_thread, &t_channel_thread, &t_alive_thread,
                &subscribe_ctx, &channel_ctx, &alive_ctx);
            if(activation_state==ACTIVATIONSTATE_TERMINATED) {
                break;
            } else  if(activation_state==ACTIVATIONSTATE_DISPATCHER_SUCCESS) {
                proxy_subscribe_thread_table_set(proxy_activator_shm->proxy_name, t_subscribe_thread, subscribe_ctx);
                proxy_channel_thread_table_set(proxy_activator_shm->proxy_name, t_channel_thread, channel_ctx);
                proxy_alive_thread_table_set(proxy_activator_shm->proxy_name, t_alive_thread, alive_ctx);
                //drain client-proxyname-table for pending request
                pthread_mutex_lock(&channel_ctx->lock);
                pthread_cond_signal(&channel_ctx->wakeup);
                pthread_mutex_unlock(&channel_ctx->lock);
                //notify client
                client_proxy_alive_notification(proxy_activator_shm->proxy_name, true);
            }
        }
    }//while(!ctx->stop) {
    pthread_mutex_unlock(&proxy_activator_shm->lock);

    pthread_attr_destroy(&thread_attr);
    gonggo_log("INFO", "proxy activation listener thread is stopped");
    pthread_exit(NULL);
}

void proxy_activator_waitfor_started(void) {
	while(!proxy_activator_started) {
		usleep(1000);
	}
}

bool proxy_activator_isstarted(void) {
    return proxy_activator_started;
}

void proxy_activator_stop(void) {
    if(pthread_mutex_lock(&proxy_activator_shm->lock)==EOWNERDEAD) {
        pthread_mutex_consistent(&proxy_activator_shm->lock);//resurrection
    }
    proxy_activator_shm->state = ACTIVATION_TERMINATION;
    pthread_cond_signal(&proxy_activator_shm->dispatcher_wakeup);
    proxy_activator_end = true;
    pthread_mutex_unlock(&proxy_activator_shm->lock);
}

const char *proxy_activator_get_gonggo_path(void) {
    return proxy_activator_gonggo_path;
}

static bool proxy_activator_shm_create(const char *gonggo_path) {
    int fd;
    char buff[GONGGOLOGBUFLEN];
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    fd = shm_open(gonggo_path, O_RDWR, S_IRUSR | S_IWUSR);
    if( errno == ENOENT ) {
        fd = shm_open(gonggo_path, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
        if(fd>-1) {
            ftruncate(fd, sizeof(ProxyActivationShm)); //set size
        }
    }

    if(fd==-1) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log("ERROR", "shm %s creation failed, %s", gonggo_path, buff);
        return false;
    }

    proxy_activator_shm = (ProxyActivationShm*)mmap(NULL, sizeof(ProxyActivationShm), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(proxy_activator_shm==MAP_FAILED) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log("ERROR", "shm %s map failed, %s", gonggo_path, buff);
        close(fd);
        return false;
    }

    close(fd);

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_SHARED);
    pthread_mutexattr_setrobust(&mutexattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_NORMAL);
    if(pthread_mutex_init(&proxy_activator_shm->lock, &mutexattr) == EBUSY) {
        if(pthread_mutex_consistent(&proxy_activator_shm->lock) == 0) {
            pthread_mutex_unlock(&proxy_activator_shm->lock);
        }
    }
    pthread_mutexattr_destroy(&mutexattr);//mutexattr is no longer needed

    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&proxy_activator_shm->dispatcher_wakeup, &condattr);
    pthread_cond_init(&proxy_activator_shm->proxy_wakeup, &condattr);
    pthread_condattr_destroy(&condattr);//condattr is no longer needed

    proxy_activator_shm->proxy_name[0] = 0;
    proxy_activator_shm->state = ACTIVATION_INIT;

    return true;
}

static void proxy_activator_shm_idle(void) {
    proxy_activator_shm->proxy_name[0] = 0;
    proxy_activator_shm->state = ACTIVATION_IDLE;
}

static enum ProxyActivationRespondState proxy_activation(
    ProxyActivationShm *shm,
    pthread_attr_t *thread_attr, pthread_t *t_subscribe_thread, pthread_t *t_channel_thread, pthread_t *t_alive_thread,
    ProxySubscribeContext** subscribe_ctx, ProxyChannelContext** channel_ctx, ProxyAliveContext** alive_ctx)
{
    struct timespec ts;
    enum ProxyActivationRespondState state;
    const char *sact;
    int status;
    ProxyChannelShm *channel_shm;
    ProxySubscribeShm *subscribe_shm;
    ProxyAliveMutexShm *alive_shm;

    *subscribe_ctx = NULL;
    *channel_ctx = NULL;
    *alive_ctx = NULL;    
    state = ACTIVATIONSTATE_DISPATCHER_FAILED;

    gonggo_log("INFO", "proxy %s activation is started", shm->proxy_name);

    if( proxy_thread_zombie(shm->proxy_name) ) {
        gonggo_log("INFO", "proxy %s activation zombie detected", shm->proxy_name);
        shm->state = ACTIVATION_PROXY_DYING;
        sact = "ACTIVATION_PROXY_DYING";
    } else {
        proxy_thread_kill(shm->proxy_name);

        channel_shm = proxy_channel_shm_get(shm->proxy_name);
        subscribe_shm = channel_shm!=NULL ? proxy_subscribe_shm_get(shm->proxy_name) : NULL;
        alive_shm = subscribe_shm!=NULL ? proxy_alive_shm_get(shm->proxy_name) : NULL;

        if(channel_shm==NULL || subscribe_shm==NULL || alive_shm==NULL) {
            if(channel_shm!=NULL) {
                munmap(channel_shm, sizeof(ProxyChannelShm));
            }
            if(subscribe_shm!=NULL) {
                munmap(subscribe_shm, sizeof(ProxySubscribeShm));
            }
            if(alive_shm!=NULL) {
                munmap(alive_shm, sizeof(ProxyAliveMutexShm));
            }
            gonggo_log("ERROR", "proxy %s activation shared memory test is failed", shm->proxy_name);
            return state;                   
        }

        shm->state = ACTIVATION_FAILED;
        sact = "ACTIVATION_FAILED";
        do {                        
            *subscribe_ctx = proxy_subscribe_context_create(shm->proxy_name, subscribe_shm);
            if( pthread_create(t_subscribe_thread, thread_attr, proxy_subscribe, *subscribe_ctx ) != 0 ) {
                gonggo_log("ERROR", "cannot create subscriber thread for proxy %s activation", shm->proxy_name);
                break;
            }

            *channel_ctx = proxy_channel_context_create(shm->proxy_name, channel_shm);
            if( pthread_create(t_channel_thread, thread_attr, proxy_channel, *channel_ctx ) != 0 ) {
                gonggo_log("ERROR", "cannot create channel thread for proxy %s activation", shm->proxy_name);
                break;
            }

            *alive_ctx = proxy_alive_context_create(shm->proxy_name, shm->pid, alive_shm);
            if( pthread_create(t_alive_thread, thread_attr, proxy_alive, *alive_ctx ) != 0 ) {
                gonggo_log("ERROR", "cannot create alive thread for proxy %s activation", shm->proxy_name);
                break;
            }

            shm->state = ACTIVATION_SUCCESS;
            sact = "ACTIVATION_SUCCESS";
            state = ACTIVATIONSTATE_DISPATCHER_SUCCESS;
        } while(false);
    }

////wait for proxy respond:BEGIN
    gonggo_log("INFO", "proxy_activation sends wakeup signal to proxy %s with status %s", shm->proxy_name, sact);
    pthread_cond_signal(&shm->proxy_wakeup); //let proxy knows whether success or failed
    pthread_mutex_unlock(&shm->lock);
    usleep(1000);    

    gonggo_log("INFO", "proxy_activation waits for lock after send wakeup signal to proxy %s", shm->proxy_name);

    if(pthread_mutex_lock(&shm->lock) == EOWNERDEAD) {//proxy died
        pthread_mutex_consistent(&shm->lock);
        state = ACTIVATIONSTATE_PROXY_DEAD;
        gonggo_log("ERROR", "proxy %s is died before releasing activation lock", shm->proxy_name);
    } else if(shm->state != ACTIVATION_DONE) {
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += PROXY_ACTIVATION_RESPOND_TIMEOUT_SECOND; //wait up to n seconds from now
        status = pthread_cond_timedwait(&shm->dispatcher_wakeup, &shm->lock, &ts);
        if(shm->state==ACTIVATION_TERMINATION) {
            state = ACTIVATIONSTATE_TERMINATED;
        } else {
            if(status == EOWNERDEAD) {
                gonggo_log("ERROR", "proxy %s is died before responding activation state", shm->proxy_name);
                pthread_mutex_consistent(&shm->lock);
                state = ACTIVATIONSTATE_PROXY_DEAD;
            } else {
                if(shm->state != ACTIVATION_DONE && state == ACTIVATIONSTATE_DISPATCHER_SUCCESS) {
                    gonggo_log("ERROR", "proxy %s activation respond is timed out", shm->proxy_name);
                    state = ACTIVATIONSTATE_PROXY_TIMEOUT;
                }
            }
        }
    }
////wait for proxy respond:END        

    if(state==ACTIVATIONSTATE_PROXY_TIMEOUT && *channel_ctx!=NULL) {
        gonggo_log("INFO", "proxy %s activation state is timeout", shm->proxy_name);                
        proxy_terminate_array_set(shm->proxy_name);
        pthread_mutex_lock(&(*channel_ctx)->lock);
        pthread_cond_signal(&(*channel_ctx)->wakeup);
        pthread_mutex_unlock(&(*channel_ctx)->lock);
    }

    if(state != ACTIVATIONSTATE_DISPATCHER_SUCCESS) {
        gonggo_log("INFO", "proxy %s activation respond is not successful with state %ld", shm->proxy_name, state);
        if(*subscribe_ctx!=NULL) {
            proxy_subscribe_stop(*subscribe_ctx);
        }

        if(*channel_ctx!=NULL) {
            proxy_channel_stop(*channel_ctx);
        }

        if(*alive_ctx!=NULL) {
            proxy_alive_stop(*alive_ctx);
        }

        if(*subscribe_ctx!=NULL) {
            pthread_join(*t_subscribe_thread, NULL);
            proxy_subscribe_context_destroy(*subscribe_ctx);
            *subscribe_ctx = NULL;
        }

        if(*channel_ctx!=NULL) {
            pthread_join(*t_channel_thread, NULL);
            proxy_channel_context_destroy(*channel_ctx);
            *channel_ctx = NULL;
        }

        if(*alive_ctx!=NULL) {
            pthread_join(*t_alive_thread, NULL);
            proxy_alive_context_destroy(*alive_ctx);
            *alive_ctx = NULL;
        }                        
    } else {
        gonggo_log("INFO", "proxy %s activation respond is successful", shm->proxy_name);
    }
 
    return state;
}