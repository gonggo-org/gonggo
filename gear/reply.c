#include <string.h>
#include <stdlib.h>
#include <civetweb.h>

#include "cJSON.h"

#include "servicestatus.h"
#include "requesttable.h"
#include "define.h"

cJSON* reply_base(const char *rid, enum ServiceStatus status) {
    cJSON *json;

    json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, RIDKEY, rid);
    cJSON_AddNumberToObject(json, "status", status);
    cJSON_AddStringToObject(json, "description", service_status(status));
    cJSON_AddStringToObject(json, "source", "gonggo");
    return json;
}

void reply_status(struct mg_connection *conn, const char *rid, enum ServiceStatus status) {
    cJSON *json;
    char *reply;

    json = reply_base(rid, status);
    reply = cJSON_Print(json);
    cJSON_Delete(json);
    mg_websocket_write(conn, MG_WEBSOCKET_OPCODE_TEXT, reply, strlen(reply));
    free(reply);
    return;
}
