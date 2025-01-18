#include <unistd.h>

#include "log.h"
#include "clienttimeout.h"
#include "clientrequesttable.h"
#include "clientconnectiontable.h"
#include "clientservicetable.h"
#include "clientproxynametable.h"
#include "clientreply.h"

//property:
static long client_timeout_period;
static volatile bool client_timeout_started = false;
static bool client_timeout_end = false;
static bool client_timeout_has_lock = false;
static pthread_mutex_t client_timeout_lock;
static pthread_cond_t client_timeout_wakeup;

void client_timeout_context_init(long period) 
{
    pthread_mutexattr_t mtx_attr;
    pthread_condattr_t condattr;

    if(!client_timeout_has_lock) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&client_timeout_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        pthread_condattr_init ( &condattr );
        pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_PRIVATE);
        pthread_cond_init(&client_timeout_wakeup, &condattr);
        pthread_condattr_destroy( &condattr );//condattr is no longer needed

        client_timeout_has_lock = true;
    }

    client_timeout_period = period;
}

void client_timeout_context_destroy() {
    if(client_timeout_has_lock) {
        pthread_mutex_destroy(&client_timeout_lock);
        pthread_cond_destroy(&client_timeout_wakeup);
        client_timeout_has_lock = false;
    }
}

void* client_timeout(void *arg) {
    struct timespec ts;
    GPtrArray *expired_request_uuid_arr;
    guint i;
    char *request_uuid;
    struct mg_connection* conn;

    gonggo_log("INFO", "db client timeout thread is started");

    client_timeout_started = true;
    pthread_mutex_lock(&client_timeout_lock);
    while(!client_timeout_end) {
        expired_request_uuid_arr = client_request_table_expired();
        for(i=0; i<expired_request_uuid_arr->len; i++) {
            request_uuid = g_ptr_array_index(expired_request_uuid_arr, i);
            conn = client_request_table_get_conn(request_uuid, false, false);
            client_service_expired_reply(conn, request_uuid, true);

            client_request_table_remove(request_uuid);            
            client_connection_table_drop(conn, request_uuid);
            client_service_table_remove(request_uuid);
            client_proxyname_table_drop_all(request_uuid);
            
            free(request_uuid);
        }
        g_ptr_array_free(expired_request_uuid_arr, false);
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += client_timeout_period; //wait n seconds from now
        pthread_cond_timedwait(&client_timeout_wakeup, &client_timeout_lock, &ts);        
    }
    pthread_mutex_unlock(&client_timeout_lock);

    gonggo_log("INFO", "db client timeout thread is stopped");
    pthread_exit(NULL);
}

void client_timeout_waitfor_started(void) {
	while(!client_timeout_started) {
		usleep(1000);
	}
}

bool client_timeout_isstarted(void) {
    return client_timeout_started;
}

void client_timeout_stop(void) {
    pthread_mutex_lock(&client_timeout_lock);
    client_timeout_end = true;
    pthread_cond_signal(&client_timeout_wakeup);
    pthread_mutex_unlock(&client_timeout_lock);    
}