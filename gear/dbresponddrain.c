#include <unistd.h>

#include "log.h"
#include "globaldata.h"
#include "dbresponddrain.h"
#include "db.h"

//property
static long db_respond_overdue;
static long db_respond_period;
static volatile bool db_respond_started = false;
static bool db_respond_end = false;
static bool db_respond_drain_has_lock = false;
static pthread_mutex_t db_respond_drain_lock;
static pthread_cond_t db_respond_drain_wakeup;

void db_respond_drain_context_init(long overdue, long period) {
    pthread_mutexattr_t mtx_attr;
    pthread_condattr_t condattr;

    if(!db_respond_drain_has_lock) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&db_respond_drain_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        pthread_condattr_init ( &condattr );
        pthread_condattr_setpshared( &condattr, PTHREAD_PROCESS_PRIVATE);
        pthread_cond_init(&db_respond_drain_wakeup, &condattr);
        pthread_condattr_destroy( &condattr );//condattr is no longer needed

        db_respond_drain_has_lock = true;
    }

    db_respond_overdue = overdue;
    db_respond_period = period;
}

void db_respond_drain_context_destroy(void) {
    if(db_respond_drain_has_lock) {
        pthread_mutex_destroy(&db_respond_drain_lock);
        pthread_cond_destroy(&db_respond_drain_wakeup);
        db_respond_drain_has_lock = false;
    }
}

void* db_respond_drain(void *arg) {
    struct timespec ts;

    gonggo_log("INFO", "db respond drain thread is started");

    db_respond_started = true;
    pthread_mutex_lock(&db_respond_drain_lock);
    while(!db_respond_end) {
        db_respond_purge(db_respond_overdue);                        
        clock_gettime(CLOCK_REALTIME, &ts);
        ts.tv_sec += db_respond_period; //wait n seconds from now
        pthread_cond_timedwait(&db_respond_drain_wakeup, &db_respond_drain_lock, &ts);        
    }
    pthread_mutex_unlock(&db_respond_drain_lock);

    gonggo_log("INFO", "db respond drain thread is stopped");
    pthread_exit(NULL);
}

void db_respond_drain_waitfor_started(void) {
	while(!db_respond_started) {
		usleep(1000);
	}
}

bool db_respond_drain_isstarted(void) {
    return db_respond_started;
}

void db_respond_drain_stop(void) {
    pthread_mutex_lock(&db_respond_drain_lock);
    db_respond_end = true;
    pthread_cond_signal(&db_respond_drain_wakeup);
    pthread_mutex_unlock(&db_respond_drain_lock);    
}