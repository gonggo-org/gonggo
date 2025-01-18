#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <civetweb.h>
#include <glib.h>

#include "cJSON.h"
#include "define.h"
#include "servicestatus.h"
#include "db.h"
#include "log.h"

#include "clientconnectiontable.h"
#include "clientproxynametable.h"
#include "clientrequesttable.h"

static void client_proxy_alive_notification_do(struct mg_connection *conn, const char *proxy_name, const GPtrArray* request_uuid_arr, bool started);

void client_proxy_alive_notification(const char *proxy_name, bool started) {
    GPtrArray *request_uuid_arr, *request_uuid_arr2;
    guint len, i;
    GHashTable *client_conn_table;
    char *request_uuid;
    struct mg_connection* conn;
    GHashTableIter hash_iter;

    request_uuid_arr = client_proxyname_table_request_dup(proxy_name, false/*get all*/);
    len = request_uuid_arr!=NULL ? request_uuid_arr->len : 0;

    client_conn_table = client_connection_table_similar();
    for(i=0; i<len; i++) {
        request_uuid = g_ptr_array_index(request_uuid_arr, i);
        conn = client_request_table_get_conn(request_uuid, false/*update_time*/, false/*remove_request*/);
        if(conn!=NULL) {
            client_connection_table_set(conn, request_uuid, client_conn_table);
        }
        free(request_uuid);
    }
    g_hash_table_iter_init(&hash_iter, client_conn_table);
    while(g_hash_table_iter_next(&hash_iter, (gpointer*)&conn, (gpointer*)&request_uuid_arr2)){
        client_proxy_alive_notification_do(conn, proxy_name, request_uuid_arr2, started);
    }  
    g_hash_table_destroy(client_conn_table);
    g_ptr_array_free(request_uuid_arr, false);
}

void client_parsing_status_reply(struct mg_connection *conn, const char *rid, enum ServiceStatus status) {
    cJSON *reply, *header;
    char *s;

    reply = cJSON_CreateObject();

    header = cJSON_AddObjectToObject(reply, CLIENTREPLY_HEADERS_KEY);    
    cJSON_AddStringToObject(header, CLIENTREPLY_SERVICE_RID_KEY, rid!=NULL ? rid : "");
    cJSON_AddNumberToObject(header, CLIENTREPLY_REQUEST_STATUS_KEY, status);

    s = cJSON_PrintUnformatted(reply);
    cJSON_Delete(reply);
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, s, strlen(s));
    free(s);
}

void client_service_reply(struct mg_connection *conn, const char *rid, cJSON* proxy_headers, cJSON* proxy_payload, bool db_save) 
{
    cJSON *reply, *headers, *child, *value;
    char *s;

    reply = cJSON_CreateObject();

    headers = cJSON_AddObjectToObject(reply, CLIENTREPLY_HEADERS_KEY);
    cJSON_AddStringToObject(headers, CLIENTREPLY_SERVICE_RID_KEY, rid);
    cJSON_AddNumberToObject(headers, CLIENTREPLY_REQUEST_STATUS_KEY, REQUEST_STATUS_ANSWERED);

    child = proxy_headers->child;
	while(child!=NULL) {//merge gonggo and proxy headers
        value = cJSON_Duplicate(cJSON_GetObjectItem(proxy_headers, child->string/*json field*/), true);
        cJSON_AddItemToObject(headers, child->string, value);
		child = child->next;
	}
    cJSON_Delete(proxy_headers);

    if(proxy_payload!=NULL) {
        cJSON_AddItemToObject(reply, CLIENTREPLY_PAYLOAD_KEY, proxy_payload);
    }

    s = cJSON_PrintUnformatted(reply);
    if(db_save && rid!=NULL && strlen(rid)>0) {
        db_respond_insert(rid, s);
    }
    cJSON_Delete(reply);
    if(conn!=NULL) {
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, s, strlen(s));
    }
    free(s);
}

void client_service_expired_reply(struct mg_connection *conn, const char *rid, bool db_save) {
    cJSON *reply, *headers;
    char *s;

    reply = cJSON_CreateObject();

    headers = cJSON_AddObjectToObject(reply, CLIENTREPLY_HEADERS_KEY);
    cJSON_AddStringToObject(headers, CLIENTREPLY_SERVICE_RID_KEY, rid);
    cJSON_AddNumberToObject(headers, CLIENTREPLY_REQUEST_STATUS_KEY, REQUEST_STATUS_EXPIRED);
    s = cJSON_PrintUnformatted(reply);
    if(db_save && rid!=NULL && strlen(rid)>0) {
        db_respond_insert(rid, s);
    }
    cJSON_Delete(reply);
    if(conn!=NULL) {
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, s, strlen(s));
    }
    free(s);    
}

static void client_proxy_alive_notification_do(struct mg_connection *conn, const char *proxy_name, const GPtrArray* request_uuid_arr, bool started) {
    cJSON *notify, *header, *payload, *rid_arr;
    char *s;    
    guint i;

    notify = cJSON_CreateObject();

    header = cJSON_AddObjectToObject(notify, CLIENTREPLY_HEADERS_KEY);
    cJSON_AddNumberToObject(header, CLIENTREPLY_REQUEST_STATUS_KEY, REQUEST_STATUS_PROXY_ALIVE_NOTIFICATION);
    cJSON_AddNumberToObject(header, CLIENTREPLY_SERVICE_STATUS_KEY, started ? PROXYSERVICESTATUS_ALIVE_START : PROXYSERVICESTATUS_ALIVE_TERMINATION);

    payload = cJSON_AddObjectToObject(notify, CLIENTREPLY_PAYLOAD_KEY);
    cJSON_AddStringToObject(payload, CLIENTREPLY_SERVICE_PROXY_KEY, proxy_name);
    rid_arr = cJSON_AddArrayToObject(payload, CLIENTREPLY_SERVICE_RID_KEY);
    for(i=0; i<request_uuid_arr->len; i++) {
        cJSON_AddItemToArray(rid_arr, cJSON_CreateString((const char*)g_ptr_array_index(request_uuid_arr, i)));
    }

    s = cJSON_PrintUnformatted(notify);
    cJSON_Delete(notify);
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, s, strlen(s));
    free(s);   
}