#include <glib.h>
#include <civetweb.h>

#include "clientrequesttable.h"

static pthread_mutex_t client_request_table_lock;
//map request-UUID to ClientRequestTableContext
static GHashTable *client_request_table = NULL;
static void client_request_table_key_destroy(gpointer data);
static void client_request_table_value_destroy(gpointer data);

void client_request_table_create(void) {
    pthread_mutexattr_t mtx_attr;

    if(client_request_table==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&client_request_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        client_request_table = g_hash_table_new_full(g_str_hash, g_str_equal, client_request_table_key_destroy, client_request_table_value_destroy);
    }
}

void client_request_table_destroy(void) {
    if(client_request_table!=NULL) {
        g_hash_table_destroy(client_request_table);
        client_request_table = NULL;
        pthread_mutex_destroy(&client_request_table_lock);
    }
}

void client_request_table_set(const char* request_uuid, struct mg_connection* conn, time_t timeoutsec) {
    ClientRequestTableContext *p;

    pthread_mutex_lock(&client_request_table_lock);
    g_hash_table_remove(client_request_table, request_uuid);
    p = (ClientRequestTableContext*)malloc(sizeof(ClientRequestTableContext));
    p->conn = conn;
    p->timestamp = time(NULL);
    p->timeoutsec = timeoutsec;
    g_hash_table_insert(client_request_table, strdup(request_uuid), p); 
    pthread_mutex_unlock(&client_request_table_lock);    
}

struct mg_connection* client_request_table_get_conn(const char* request_uuid, bool update_time, bool remove_request) {
    ClientRequestTableContext* p;
    struct mg_connection* conn = NULL;

    pthread_mutex_lock(&client_request_table_lock);
    p = (ClientRequestTableContext*)g_hash_table_lookup(client_request_table, request_uuid);
    if(p!=NULL) {
        conn = p->conn;
        if(remove_request) {
            g_hash_table_remove(client_request_table, request_uuid);
        } else if(update_time) {
            p->timestamp = time(NULL);
        }
    }    
    pthread_mutex_unlock(&client_request_table_lock);
    return conn;
}

void client_request_table_remove(const char* request_uuid) {
    pthread_mutex_lock(&client_request_table_lock);
    g_hash_table_remove(client_request_table, request_uuid);
    pthread_mutex_unlock(&client_request_table_lock);
}

GPtrArray *client_request_table_expired(void) {
    GPtrArray *request_uuid_arr;
    time_t now;
    GHashTableIter iter;
    char *request_uuid;
    ClientRequestTableContext *ctx;

    request_uuid_arr = g_ptr_array_new_full(1, (GDestroyNotify)free);
    pthread_mutex_lock(&client_request_table_lock);
    now = time(NULL);
    g_hash_table_iter_init(&iter, client_request_table);
    while(g_hash_table_iter_next(&iter, (gpointer*)&request_uuid, (gpointer*)&ctx)) {
        if(ctx->timeoutsec>0) {
            if(ctx->timestamp + ctx->timeoutsec < now) {
                g_ptr_array_add(request_uuid_arr, strdup(request_uuid));
            }
        }
    }
    pthread_mutex_unlock(&client_request_table_lock);
    return request_uuid_arr;
}

static void client_request_table_key_destroy(gpointer data) {
    if(data!=NULL) {
        free((char*)data);
    }
    return;
}

static void client_request_table_value_destroy(gpointer data) {
   if(data!=NULL) {
        ClientRequestTableContext* p = (ClientRequestTableContext*)data;
        free(p);
    }    
}