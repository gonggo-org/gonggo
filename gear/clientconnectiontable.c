#include <stdbool.h>
#include <glib.h>
#include <civetweb.h>

#include "util.h"

static pthread_mutex_t client_connection_table_lock;
//map connection object to request-UUID array
static GHashTable *client_connection_table = NULL;
static void client_connection_table_value_destroy(GPtrArray*);

void client_connection_table_create(void) {
    pthread_mutexattr_t mtx_attr;
        
    if(client_connection_table == NULL) {        
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&client_connection_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        client_connection_table = g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)client_connection_table_value_destroy);
    }
}

GHashTable *client_connection_table_similar(void) {
#ifdef g_hash_table_new_similar
    return client_connection_table !=NULL ? g_hash_table_new_similar(client_connection_table) : NULL;
#else
    return client_connection_table !=NULL ? g_hash_table_new_full(g_direct_hash, g_direct_equal, NULL, (GDestroyNotify)client_connection_table_value_destroy) : NULL;
#endif
}

void client_connection_table_destroy(void) {
    if(client_connection_table!=NULL) {
        g_hash_table_destroy(client_connection_table);
        client_connection_table = NULL;
        pthread_mutex_destroy(&client_connection_table_lock);        
    }
}

void client_connection_table_set(struct mg_connection* conn, const char* request_uuid, GHashTable *othertable_nolock) {
    GPtrArray* arr;
    bool exist = false;
    GHashTable *table;

    table = othertable_nolock!=NULL ? othertable_nolock : client_connection_table;    
    if(table==client_connection_table) {
        pthread_mutex_lock(&client_connection_table_lock);
    }
    arr = (GPtrArray*)g_hash_table_lookup(table, conn);
    if(arr==NULL) {
        arr = g_ptr_array_new_full(1, (GDestroyNotify)free);
        g_hash_table_insert(table, conn, arr); 
    } else {
        exist = g_ptr_array_find_with_equal_func(arr, request_uuid, (GEqualFunc)str_equal, NULL);
    }
    if(!exist) {
        g_ptr_array_add(arr, strdup(request_uuid));
    }
    if(table==client_connection_table) {
        pthread_mutex_unlock(&client_connection_table_lock);
    }
}

void client_connection_table_drop(struct mg_connection* conn, const char* request_uuid) {
    GPtrArray* arr;
    guint idx;

    pthread_mutex_lock(&client_connection_table_lock);
    if( (arr = (GPtrArray*)g_hash_table_lookup(client_connection_table, conn)) != NULL ) {        
        if(g_ptr_array_find_with_equal_func(arr, request_uuid, (GEqualFunc)str_equal, &idx)) {
            g_ptr_array_remove_index(arr, idx);
            if(arr->len<1) {
                g_hash_table_remove(client_connection_table, conn);
            }
        }
    }
    pthread_mutex_unlock(&client_connection_table_lock);
}

GPtrArray* client_connection_table_request_dup(const struct mg_connection* conn) {
    GPtrArray *p, *ret;

    pthread_mutex_lock(&client_connection_table_lock);
    p = (GPtrArray*)g_hash_table_lookup(client_connection_table, conn);
    ret = p!=NULL ? g_ptr_array_copy(p, (GCopyFunc)str_dup, NULL) : NULL;
    pthread_mutex_unlock(&client_connection_table_lock);
    return ret;
}

void client_connection_table_remove(const struct mg_connection* conn) {
    pthread_mutex_lock(&client_connection_table_lock);
    g_hash_table_remove(client_connection_table, conn);
    pthread_mutex_unlock(&client_connection_table_lock);
}

static void client_connection_table_value_destroy(GPtrArray* p) {
    if(p!=NULL) {
        g_ptr_array_free(p, true);
    }
}