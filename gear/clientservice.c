#include <stdlib.h>
#include <string.h>
#include <civetweb.h>

#include "cJSON.h"
#include "globaldata.h"
#include "define.h"
#include "log.h"
#include "clientreply.h"
#include "db.h"
#include "proxyactivator.h"
#include "clientconnectiontable.h"
#include "clientrequesttable.h"
#include "clientservicetable.h"
#include "clientproxynametable.h"
#include "proxychannelthreadtable.h"
#include "proxyalivethreadtable.h"
#include "strarr.h"
#include "gonggouuid.h"

static void client_service_route_cleanup(cJSON *root);
static void gonggo_test(const char *rid, struct mg_connection *conn);
static void gonggo_response_dump(const char *rid, struct mg_connection *conn,
    const char *service_name, const char *response_dump_rid, unsigned int response_dump_start, unsigned int response_dump_size);

void client_service_route(const char *remote_addr, const char *uri, const char *data, size_t datasize, struct mg_connection *conn) {
    cJSON *root, *headers, *payload;
    const cJSON *item;
    const char *gonggo_path, *rid, *proxy_name, *service_name;
    time_t timeout;
    const ProxyChannelTableContext *proxy_channel_table_ctx;
    ProxyChannelContext *proxy_channel_ctx;

    const char *response_dump_rid;
    int response_dump_start, response_dump_size;

    gonggo_path = proxy_activator_get_gonggo_path();

    if( gonggo_path==NULL || strcmp(uri, gonggo_path) != 0 ) {
        return;
    }

    root = cJSON_Parse(data);
    if(root==NULL) {
        gonggo_log("ERROR", "%s sends empty message", remote_addr);
        return;
    }

    headers = cJSON_GetObjectItem(root, SERVICE_HEADERS_KEY);
    if(headers==NULL) {
        gonggo_log("ERROR", "%s message does not have headers", remote_addr);
        client_parsing_status_reply(conn, NULL, REQUEST_PARSE_STATUS_HEADERS_MISSING);
        client_service_route_cleanup(root);
        return;        
    }

    item = cJSON_GetObjectItem(headers, SERVICE_RID_KEY);
    rid = item!=NULL ? item->valuestring : NULL;
    if( rid==NULL || strlen(rid)<1 ) {
        gonggo_log("ERROR", "%s message does not have request id", remote_addr);
        client_parsing_status_reply(conn, NULL, REQUEST_PARSE_STATUS_RID_MISSING);
        client_service_route_cleanup(root);
        return;
    }

    if(strlen(rid) > (UUIDBUFLEN-1)) {
        gonggo_log("ERROR", "%s message request id length exceed %d", remote_addr, UUIDBUFLEN-1);
        client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_RID_OVERLENGTH);
        client_service_route_cleanup(root);
        return;
    }

    item = cJSON_GetObjectItem(headers, SERVICE_PROXY_KEY);
    proxy_name = item!=NULL ? item->valuestring : NULL;

    item = cJSON_GetObjectItem(headers, SERVICE_SERVICE_KEY);
    service_name = item!=NULL ? item->valuestring : NULL;
    if(service_name==NULL || strlen(service_name)<1) {
        gonggo_log("ERROR", "%s message headers does not have service name", remote_addr);
        client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_SERVICE_MISSING);
        client_service_route_cleanup(root);
        return;
    }    

    if(strcmp(service_name, GONGGOSERVICE_REQUEST_DROP)==0) {
        gonggo_log("ERROR", "%s message service name %d is reserved", remote_addr, service_name);
        client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_SERVICE_RESERVED);
        client_service_route_cleanup(root);
        return;
    }

    payload = cJSON_GetObjectItem(root, SERVICE_PAYLOAD_KEY);

    if(proxy_name==NULL) {
        if(strcmp(service_name, "test")==0) {
            client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_ACKNOWLEDGED);
            gonggo_test(rid, conn);
            client_service_route_cleanup(root);
            return;
        } else if(strcmp(service_name, "responseDump")==0) {
            if(payload==NULL) {
                gonggo_log("ERROR", "%s query message does not have payload", remote_addr);            
                client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_PAYLOAD_MISSING);
                client_service_route_cleanup(root);
                return;
            }
            item = cJSON_GetObjectItem(payload, SERVICE_RID_KEY);
            response_dump_rid = item!=NULL ? item->valuestring : NULL;
            item = cJSON_GetObjectItem(payload, "start");
            response_dump_start = item!=NULL ? item->valueint : 0;
            item = cJSON_GetObjectItem(payload, "size");
            response_dump_size = item!=NULL ? item->valueint : 0;        
            if( response_dump_rid==NULL || strlen(response_dump_rid)<1
                || response_dump_start<1
                || response_dump_size<1
            ) {
                gonggo_log("ERROR", "%s query message does not have valid payload", remote_addr);
                client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_PAYLOAD_INVALID);
                client_service_route_cleanup(root);
                return;
            }
            client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_ACKNOWLEDGED);
            gonggo_response_dump(rid, conn, service_name, response_dump_rid, response_dump_start, response_dump_size);
            client_service_route_cleanup(root);
            return;
        } else {
            gonggo_log("ERROR", "%s message headers contains invalid service name %s", remote_addr, service_name);
            client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_GONGGO_SERVICE_INVALID);
            client_service_route_cleanup(root);
            return;
        }
    }//if(proxy_name==NULL)

////save to table:BEGIN
    item = cJSON_GetObjectItem(headers, CLIENTREPLY_TIMEOUT_KEY);
    if(item==NULL){
        gonggo_log("ERROR", "%s query message headers does not have timeout", remote_addr);            
        client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_TIMEOUT_MISSING);
        client_service_route_cleanup(root);
        return;        
    }
    timeout = (time_t)item->valuedouble;
    if(timeout<1) {
        gonggo_log("ERROR", "%s query message headers contains invalid timeout %ld", remote_addr, timeout);            
        client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_TIMEOUT_INVALID);
        client_service_route_cleanup(root);
        return;
    }
    client_parsing_status_reply(conn, rid, REQUEST_PARSE_STATUS_ACKNOWLEDGED);
    client_request_table_set(rid, conn, timeout);
    client_connection_table_set(conn, rid, NULL);
    client_service_table_set(rid, service_name, payload);
    client_proxyname_table_set(proxy_name, rid, NULL);
////save to table:END

////wake up proxy channel thread if available:BEGIN
    proxy_channel_table_ctx = proxy_channel_thread_table_get(proxy_name);
    proxy_channel_ctx = proxy_channel_table_ctx != NULL ? proxy_channel_table_ctx->ctx : NULL;
    if(proxy_channel_ctx!=NULL) {
        pthread_mutex_lock(&proxy_channel_ctx->lock);
        pthread_cond_signal(&proxy_channel_ctx->wakeup);
        pthread_mutex_unlock(&proxy_channel_ctx->lock);
    }
////wake up proxy channel thread if available:END

    client_service_route_cleanup(root);
}

void client_service_drop_conn(const struct mg_connection *conn) {
    GPtrArray* request_uuid_arr;
    guint idx;
    char *request_uuid, *proxy_name;
    GHashTable *client_proxyname_table;
    GHashTableIter iter;
    ClientProxynameTableValue *client_proxyname_value;
    const ProxyChannelTableContext *proxy_channel_table_ctx;
    ProxyChannelContext *proxy_channel_ctx;
    char rid[UUIDBUFLEN];
    cJSON *payload;

    request_uuid_arr = client_connection_table_request_dup(conn);    
    if(request_uuid_arr==NULL) {
        return;
    }

    client_proxyname_table = client_proxyname_table_similar();
    for(idx=0; idx<request_uuid_arr->len; idx++) {
        request_uuid = (char*)g_ptr_array_index(request_uuid_arr, idx);

        proxy_name = client_proxyname_table_proxy_name_dup(request_uuid);
        if(proxy_name!=NULL) {
            client_proxyname_table_set(proxy_name, request_uuid, client_proxyname_table);
            free(proxy_name);
        }

        client_request_table_remove(request_uuid);
        client_service_table_remove(request_uuid);
        client_proxyname_table_drop_all(request_uuid);
        free(request_uuid);
    }

    g_hash_table_iter_init(&iter, client_proxyname_table);
    while (g_hash_table_iter_next(&iter, (gpointer*)&proxy_name, (gpointer*)&client_proxyname_value)) {
        proxy_channel_table_ctx = proxy_channel_thread_table_get(proxy_name);
        proxy_channel_ctx = proxy_channel_table_ctx != NULL ? proxy_channel_table_ctx->ctx : NULL;

        if(proxy_channel_ctx!=NULL) {
            for(idx=0; idx<client_proxyname_value->request_uuids->len; idx++) {
                gonggo_uuid_generate(rid);
                payload = cJSON_CreateObject();
                request_uuid = (char*)g_ptr_array_index(client_proxyname_value->request_uuids, idx);
                cJSON_AddItemToObject(payload, SERVICE_RID_KEY, cJSON_CreateString(request_uuid));
                client_service_table_set(rid, GONGGOSERVICE_REQUEST_DROP, payload);
                client_proxyname_table_set(proxy_name, rid, NULL);    
                cJSON_Delete(payload);
            }
            pthread_mutex_lock(&proxy_channel_ctx->lock);
            pthread_cond_signal(&proxy_channel_ctx->wakeup);
            pthread_mutex_unlock(&proxy_channel_ctx->lock);
        }
    }
        
    client_connection_table_remove(conn);
    g_ptr_array_free(request_uuid_arr, false);
    g_hash_table_destroy(client_proxyname_table);
}

static void client_service_route_cleanup(cJSON *root) {
    if(root!=NULL) {
        cJSON_Delete(root);
    }
}

static void gonggo_test(const char *rid, struct mg_connection *conn)
{
    cJSON *headers, *payload, *arr;
    StrArr sa;
    unsigned int i;
    char *proxy_name;

    headers = cJSON_CreateObject();
    cJSON_AddNumberToObject(headers, CLIENTREPLY_SERVICE_STATUS_KEY, GONGGO_TEST_SUCCESS);    

    payload = cJSON_CreateObject();

    arr = cJSON_AddArrayToObject(payload, "services");
    cJSON_AddItemToArray(arr, cJSON_CreateString("test"));
    cJSON_AddItemToArray(arr, cJSON_CreateString("responseDump"));

    arr = cJSON_AddArrayToObject(payload, "proxies");
    proxy_alive_thread_table_char_keys_create(&sa);
    for(i=0; i<sa.len; i++) {
        proxy_name = sa.arr[i];
        cJSON_AddItemToArray(arr, cJSON_CreateString(proxy_name));
        free(proxy_name);
    }    
    str_arr_destroy(&sa, false);

    client_service_reply(conn, rid, headers, payload, true);
}

static void gonggo_response_dump(const char *rid, struct mg_connection *conn,
    const char *service_name, const char *dump_rid, unsigned int start, unsigned int size) {

    RespondQueryResult r;
    cJSON *headers, *payload, *arr;
    char **run;
    unsigned int status;

    payload = cJSON_CreateObject();
    db_respond_query_result_init(&r);
    if(db_respond_query(dump_rid, start, size, &r)){
        arr = cJSON_AddArrayToObject(payload, "responses");
        if( (run = r.respond)!=NULL ) {
            while(*run!= NULL) {
                cJSON_AddItemToArray(arr, cJSON_Parse(*run));
                run++;
            }
        }
        cJSON_AddNumberToObject(payload, "stop", r.stop);
        cJSON_AddNumberToObject(payload, "more", r.more);
        status = GONGGO_RESPONSE_DUMP_SUCCESS;
    } else {
        status = GONGGO_RESPONSE_DUMP_DB_ERROR;
    }
    db_respond_query_result_destroy(&r);
    headers = cJSON_CreateObject();
    cJSON_AddNumberToObject(headers, CLIENTREPLY_SERVICE_STATUS_KEY, status);

    client_service_reply(conn, rid, headers, payload, false);//do not save into db
}