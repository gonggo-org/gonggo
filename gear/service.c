#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */
#include <civetweb.h>

#include "cJSON.h"

#include "proxychannel.h"
#include "servicestatus.h"
#include "globaldata.h"
#include "log.h"
#include "confvar.h"
#include "reply.h"
#include "requesttable.h"

static void parse_sawang_path(char *p, char **sawang_name, char **task);
static void test(const DbContext *db_ctx, const LogContext *log_ctx, const char *rid, struct mg_connection *conn);
static void respond(const DbContext *db_ctx, const LogContext *log_ctx, int query_size, const char *rid, struct mg_connection *conn, const cJSON *root);

void service_route(const char *uri, const char *data, size_t datasize, const GlobalData *global_data, struct mg_connection *conn) {
    char *service, *sawang_name, *task, *rid;
    size_t len;
    cJSON *root;
    const cJSON *item, *arg;

    if( strcmp(uri, global_data->activation_path) != 0 )
        return;

    root = cJSON_Parse(data);
    if(root!=NULL) {
        item = cJSON_GetObjectItem(root, RIDKEY);
        rid = item!=NULL ? item->valuestring : NULL;
        if(rid!=NULL) {
            if( strlen(rid) > (UUIDBUFLEN-1) ) {
                gonggo_log(global_data->log_ctx, "ERROR", "rid length exceed %d", UUIDBUFLEN-1);
                reply_status(conn, rid, SERVICE_STATUS_RID_OVERLENGTH);
            } else {
                item = cJSON_GetObjectItem(root, "service");
                if(item!=NULL) {
                    if( (len = strlen(item->valuestring)) > 0 ) {
                        service = strdup( item->valuestring );
                        parse_sawang_path(service, &sawang_name, &task);
                        if( task==NULL && strcmp(sawang_name, "test")==0 ) {
                            reply_status(conn, rid, SERVICE_STATUS_REQUEST_ACKNOWLEDGED);
                            test(global_data->db_ctx, global_data->log_ctx, rid, conn);
                        } else if( task==NULL && strcmp(sawang_name, "respond")==0 ) {
                            reply_status(conn, rid, SERVICE_STATUS_REQUEST_ACKNOWLEDGED);
                            arg = cJSON_GetObjectItem(root, "arg");
                            respond(global_data->db_ctx, global_data->log_ctx, global_data->respond_query_size, rid, conn, arg);
                        } else {
                            if( strlen(sawang_name) > (SHMPATHBUFLEN-1) ) {
                                gonggo_log(global_data->log_ctx, "ERROR", "proxy name length exceed %d", SHMPATHBUFLEN-1);
                                reply_status(conn, rid, SERVICE_STATUS_TASK_OVERLENGTH);
                            } else if(task==NULL) {
                                gonggo_log(global_data->log_ctx, "ERROR", "empty task");
                                reply_status(conn, rid, SERVICE_STATUS_TASK_EMPTY);
                            } else if(strlen(task) > (TASKBUFLEN-1) ) {
                                gonggo_log(global_data->log_ctx, "ERROR", "task length exceed %d", TASKBUFLEN-1);
                                reply_status(conn, rid, SERVICE_STATUS_TASK_OVERLENGTH);
                            } else {
                                arg = cJSON_GetObjectItem(root, "arg");
                                if( proxy_channel(rid, global_data->log_ctx, conn, sawang_name, task, arg,
                                        global_data->proxy_thread_table, global_data->mtx_thread_table
                                    )
                                )
                                    request_table_entry(global_data->request_table, global_data->mtx_request_table, rid, conn, sawang_name);
                            }
                        }
                        free(service);
                    }
                }
            }
        }
        cJSON_Delete(root);
    }

    return;
}

static void parse_sawang_path(char *p, char **sawang_name, char **task) {
    char *c;

    c = *p == '/' ? p + 1 : p;
    *sawang_name = c;
    *task = NULL;

    c = strchr(c, '/');
    if(c!=NULL) {
        *c = 0;
        *task = c+1;
        if(strlen(*task)==0)
            *task = NULL;
    }

    return;
}

static void test(const DbContext *db_ctx, const LogContext *log_ctx, const char *rid, struct mg_connection *conn) {
    cJSON *json;
    char *reply;

    json = reply_base(rid, SERVICE_STATUS_REQUEST_ANSWERED);
    reply = cJSON_Print(json);
    cJSON_Delete(json);
    db_respond_insert(db_ctx, log_ctx, rid, reply);
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, reply, strlen(reply));
    free(reply);
    return;
}

static void respond(const DbContext *db_ctx, const LogContext *log_ctx, int query_size, const char *rid, struct mg_connection *conn, const cJSON *arg) {
    const cJSON *query, *start;
    cJSON *json, *result, *arr;
    char *reply;
    RespondQueryResult query_result;
    char **run;

    query = cJSON_GetObjectItem(arg, "rid");
    start = cJSON_GetObjectItem(arg, "start");

    json = NULL;
    db_respond_query_result_init(&query_result);

    do {
        if( !cJSON_IsString(query) || !cJSON_IsNumber(start) ) {
            json = reply_base(rid, SERVICE_STATUS_INVALID_PAYLOAD);
            break;
        }

        if( !db_respond_query(db_ctx, log_ctx, query->valuestring, start->valueint, query_size, &query_result) ) {
            json = reply_base(rid, SERVICE_STATUS_INTERNAL_ERROR);
            break;
        }

        json = reply_base(rid, SERVICE_STATUS_REQUEST_ANSWERED);
        result = cJSON_AddObjectToObject(json, "result");
        arr = cJSON_AddArrayToObject(result, "respond");
        if( query_result.respond!=NULL ) {
            run = query_result.respond;
            while ( *run != NULL ) {
                cJSON_AddItemToArray(arr, cJSON_Parse(*run));
                run++;
            }
        }
        cJSON_AddNumberToObject(result, "stop", query_result.stop);
        cJSON_AddNumberToObject(result, "more", query_result.more);
    } while(false);

    if(json!=NULL) {
        reply = cJSON_Print(json);
        cJSON_Delete(json);
        mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, reply, strlen(reply));
        free(reply);
    }

    db_respond_query_result_destroy(&query_result);
}
