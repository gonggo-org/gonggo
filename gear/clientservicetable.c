#include <glib.h>
#include <stdbool.h>

#include "clientservicetable.h"

static pthread_mutex_t client_service_table_lock;
//map service-UUID to service payload
static GHashTable *client_service_table = NULL;

void client_service_table_create(void) {
    pthread_mutexattr_t mtx_attr;

    if(client_service_table==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&client_service_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);
        
        client_service_table = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)free, (GDestroyNotify)free);
    }
}

void client_service_table_destroy(void) {
    if(client_service_table!=NULL) {
        g_hash_table_destroy(client_service_table);
        client_service_table = NULL;
        pthread_mutex_destroy(&client_service_table_lock);
    }
}

char* client_service_table_service_payload_create(const char *service_name, const cJSON *payload) {
    cJSON *j;
    char *s;

    j = cJSON_CreateObject();
    cJSON_AddItemToObject(j, SERVICE_SERVICE_KEY, cJSON_CreateString(service_name));
    if(payload!=NULL) {        
        cJSON_AddItemToObject(j, SERVICE_PAYLOAD_KEY,  cJSON_Duplicate(payload, true));
    }
    s = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);

    return s;
}

void client_service_table_set(const char* request_uuid, const char *service_name, const cJSON *payload) {
    char *service_payload;

    pthread_mutex_lock(&client_service_table_lock);
    g_hash_table_remove(client_service_table, request_uuid);
    service_payload = client_service_table_service_payload_create(service_name, payload);
    g_hash_table_insert(client_service_table, strdup(request_uuid), service_payload); 
    pthread_mutex_unlock(&client_service_table_lock);    
}

char *client_service_table_dup(const char* request_uuid) {
    char *s;

    pthread_mutex_lock(&client_service_table_lock);
    s = g_hash_table_lookup(client_service_table, request_uuid);
    if(s!=NULL) {
        s = strdup(s);
    }
    pthread_mutex_unlock(&client_service_table_lock);

    return s;
}

void client_service_table_remove(const char* request_uuid) {
    pthread_mutex_lock(&client_service_table_lock);
    g_hash_table_remove(client_service_table, request_uuid);
    pthread_mutex_unlock(&client_service_table_lock);
}