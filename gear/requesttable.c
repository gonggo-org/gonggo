#include "requesttable.h"
#include "proxyheartbeat.h"
#include "proxychannel.h"

typedef struct RequestTableValue {
    struct mg_connection *conn;
    char sawang_name[SHMPATHBUFLEN];
    time_t timestamp;
} RequestTableValue;

typedef struct ConnSearchData {
    const struct mg_connection *conn;
    GHashTable *table;//map sawang_name to list of request id
} ConnSearchData;

typedef struct RequestRemoveFromProxy {
    const LogContext *log_ctx;
    GHashTable *proxy_thread_table;
    pthread_mutex_t *mtx_thread_table;
} RequestRemoveFromProxy;

static void request_table_key_destroy(gpointer data);
static void request_table_value_destroy(gpointer data);
static gboolean conn_search(gpointer key, gpointer value, gpointer user_data);

static void request_remove_from_proxy(gpointer key, gpointer value, gpointer user_data);
static void sawang_table_key_destroy(gpointer data);
static void sawang_table_value_destroy(gpointer data);
static void sawang_table_item_free(gpointer item);

GHashTable *request_table_create() {
    return g_hash_table_new_full(g_str_hash, g_str_equal, request_table_key_destroy, request_table_value_destroy);
}

void request_table_destroy(GHashTable *request_table) {
    g_hash_table_destroy(request_table);
}

void request_table_entry(GHashTable *request_table, pthread_mutex_t *lock,
    const char *rid, struct mg_connection *conn, const char *sawang_name) {

    RequestTableValue *p_value;

    p_value = (RequestTableValue*)malloc(sizeof(RequestTableValue));
    p_value->conn = conn;
    strcpy( p_value->sawang_name, sawang_name);
    p_value->timestamp = time(NULL);
    pthread_mutex_lock(lock);
    g_hash_table_insert(request_table, strdup(rid), p_value);
    pthread_mutex_unlock(lock);

    return;
}

void request_table_conn_remove(
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    const struct mg_connection *conn, const LogContext *log_ctx)
{
    ConnSearchData searchData;
    RequestRemoveFromProxy requestRemove;

    searchData.conn = conn;
    searchData.table = g_hash_table_new_full(g_str_hash, g_str_equal, sawang_table_key_destroy, sawang_table_value_destroy);

    pthread_mutex_lock(mtx_request_table);
    g_hash_table_foreach_remove(request_table, conn_search, (gpointer)&searchData);
    pthread_mutex_unlock(mtx_request_table);

    requestRemove.log_ctx = log_ctx;
    requestRemove.proxy_thread_table = proxy_thread_table;
    requestRemove.mtx_thread_table = mtx_thread_table;

    g_hash_table_foreach(searchData.table, request_remove_from_proxy, (gpointer)&requestRemove);
    g_hash_table_destroy(searchData.table);
    return;
}

static void request_remove_from_proxy(gpointer key, gpointer value, gpointer user_data) {
    RequestRemoveFromProxy *requestRemove;

    requestRemove = (RequestRemoveFromProxy*)user_data;
    proxy_channel_dead_request(
        (const char*)key, //sawang_name
        (GSList*)value, //request_id_list
        requestRemove->log_ctx, requestRemove->proxy_thread_table, requestRemove->mtx_thread_table);
}

struct mg_connection* request_table_conn_get(GHashTable *request_table, pthread_mutex_t *lock,
    const char *rid, bool update_timestamp, bool remove_request)
{
    gpointer pointer;
    RequestTableValue *value;
    struct mg_connection *conn;

    pthread_mutex_lock( lock );
    pointer = g_hash_table_lookup(request_table, rid);
    value = pointer!=NULL ? (RequestTableValue*)pointer : NULL;
    if(value!=NULL && update_timestamp && !remove_request)
        value->timestamp = time(NULL);
    conn = value!=NULL ? value->conn : NULL;
    if(remove_request)
        g_hash_table_remove(request_table, rid);
    pthread_mutex_unlock( lock );
    return conn;
}

static void request_table_key_destroy(gpointer data) {
    if(data!=NULL)
        free((char*)data);
    return;
}

static void request_table_value_destroy(gpointer data) {
    if(data!=NULL)
        free((RequestTableValue*)data);
    return;
}

static gboolean conn_search(gpointer key, gpointer value, gpointer user_data) {
    ConnSearchData *searchData;
    RequestTableValue *request_table_value;
    gboolean match;
    gpointer pointer;
    GSList *list;

    if(value!=NULL) {
        searchData = (ConnSearchData*)user_data;
        request_table_value = (RequestTableValue*)value;
        match = request_table_value->conn == searchData->conn;
        if(match) {
            pointer = g_hash_table_lookup(searchData->table, request_table_value->sawang_name);
            if(pointer==NULL)
                g_hash_table_insert(searchData->table,
                    strdup(request_table_value->sawang_name),
                    g_slist_append(NULL, strdup( (char*)key ) )
                );
            else {
                list = (GSList*)pointer;
            ////following is ugly way to silent warn_unused_result on compiling
                if( g_slist_append(list, strdup( (char*)key ) ) );
            }
        }
        return match;
    }

    return FALSE;
}

static void sawang_table_key_destroy(gpointer data) {
    if(data!=NULL)
        free((char*)data);
    return;
}

static void sawang_table_value_destroy(gpointer data) {
    if(data!=NULL)
        g_slist_free_full( (GSList*)data, sawang_table_item_free);
    return;
}

static void sawang_table_item_free(gpointer item) {
    if( item!=NULL )
        free( (char*)item );
}
