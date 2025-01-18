#include <errno.h>
#include <unistd.h>
#include <glib.h>

#include "log.h"
#include "proxy.h"
#include "clientproxynametable.h"
#include "globaldata.h"

//property
static GQueue *proxy_terminator_queue = NULL;
static pthread_mutex_t proxy_terminator_lock;
static pthread_cond_t proxy_terminator_wakeup;
static volatile bool proxy_terminator_started = false;
static bool proxy_terminator_end = false;

void proxy_terminator_context_init(void) {
    pthread_mutexattr_t mutexattr;
    pthread_condattr_t condattr;

    pthread_mutexattr_init(&mutexattr);
    pthread_mutexattr_setpshared(&mutexattr, PTHREAD_PROCESS_PRIVATE);
    pthread_mutexattr_setrobust(&mutexattr, PTHREAD_MUTEX_ROBUST);
    pthread_mutexattr_settype(&mutexattr, PTHREAD_MUTEX_NORMAL);
    if(pthread_mutex_init(&proxy_terminator_lock, &mutexattr) == EBUSY) {
        if(pthread_mutex_consistent(&proxy_terminator_lock) == 0) {
            pthread_mutex_unlock(&proxy_terminator_lock);
        }
    }
    pthread_mutexattr_destroy(&mutexattr);//mutexattr is no longer needed

    pthread_condattr_init(&condattr);
    pthread_condattr_setpshared(&condattr, PTHREAD_PROCESS_PRIVATE);
    pthread_cond_init(&proxy_terminator_wakeup, &condattr);
    pthread_condattr_destroy(&condattr);//condattr is no longer needed   

    proxy_terminator_queue = g_queue_new();
}

void proxy_terminator_context_destroy(void) {
    pthread_mutex_destroy(&proxy_terminator_lock);
    pthread_cond_destroy(&proxy_terminator_wakeup);
    g_queue_free_full(proxy_terminator_queue, (GDestroyNotify)free);
    proxy_terminator_queue = NULL;
}

void* proxy_terminator(void *arg) {
    char *proxy_name;

    gonggo_log("INFO", "%s starts proxy terminator", gonggo_name);
    proxy_terminator_started = true;

    pthread_mutex_lock(&proxy_terminator_lock);

    while(!proxy_terminator_end) {
        pthread_cond_wait(&proxy_terminator_wakeup, &proxy_terminator_lock);
        while((proxy_name=g_queue_pop_head(proxy_terminator_queue))!=NULL) {
            //set back to unsent
            //so when the proxt back to live
            //all the requests will be picked up on the next proxy_channel and re-dispatch to the proxy
            client_proxyname_table_request_unsent(proxy_name);
            gonggo_log("INFO", "%s proxy terminator thread kill %s threads", gonggo_name, proxy_name);
            proxy_thread_kill(proxy_name);
            //should has been set in void* proxy_alive(void *arg)
            proxy_thread_terminating_drop(proxy_name);
            free(proxy_name);
        }
    }

    pthread_mutex_unlock(&proxy_terminator_lock);

    gonggo_log("INFO", "%s stops proxy terminator", gonggo_name);
    pthread_exit(NULL);
}

void proxy_terminator_waitfor_started(void) {
	while(!proxy_terminator_started) {
		usleep(1000);
	}
}

bool proxy_terminator_isstarted(void) {
    return proxy_terminator_started;
}

void proxy_terminator_stop(void) {
    pthread_mutex_lock(&proxy_terminator_lock);
    proxy_terminator_end = true;
    pthread_cond_signal(&proxy_terminator_wakeup);
    pthread_mutex_unlock(&proxy_terminator_lock);
}

void proxy_terminator_awake(const char *proxy_name) {
    pthread_mutex_lock(&proxy_terminator_lock);
    g_queue_push_tail(proxy_terminator_queue, strdup(proxy_name));
    pthread_cond_signal(&proxy_terminator_wakeup);
    pthread_mutex_unlock(&proxy_terminator_lock);    
}