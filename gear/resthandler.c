#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <civetweb.h>
#include <cJSON.h>

#include "log.h"
#include "resthandler.h"
#include "proxychannelthreadtable.h"

static void rest_param_parse(char *s, cJSON *json);

int rest_handler(struct mg_connection *conn, void *cbdata) {
    const struct mg_request_info *ri;
    const char *url, *method, *content_type;
    char buffer[1024], *payload, *p_url, *run, *p;
    int read_len, payload_len, write_offset, i;
    const char *gonggo, *proxy, *endpoint;
    char *param;
    RestHandlerData *rhd;
    const ProxyChannelTableContext *proxy_channel_table_ctx;
    cJSON *payload_json, *param_json;
    RestRespond rest_respond;

    rhd = (RestHandlerData*)cbdata;

    ri = mg_get_request_info(conn);
    url = ri->local_uri;
    method = ri->request_method;

    p_url = strdup(url+1);//skip first '/'
    run = p_url; 

    gonggo = run;
    proxy = NULL;
    endpoint = NULL;    
    param = NULL;

    p = strchr(run, '/');
    if(p!=NULL) {
        *p = 0;        
        run = p + 1;
    ////find proxy and endpoint
        proxy = run;
        p = strchr(run, '/');
        if(p!=NULL) {
            *p = 0; 
            endpoint = p + 1;
        }
    }

    if(strcmp(gonggo, rhd->gonggo_name)!=0) {        
        mg_send_http_error(conn, 400, "path %s root is not %s\n", url, rhd->gonggo_name);
        free(p_url);
        return 400;
    }

    if(proxy==NULL) {        
        mg_send_http_error(conn, 400, "path %s does not contain proxy\n", url);
        free(p_url);
        return 400;
    }

    if(endpoint==NULL) {
        mg_send_http_error(conn, 400, "path %s does not contain endpoint\n", url);
        free(p_url);
        return 400;
    }

    proxy_channel_thread_table_lock_hold(true);
    if((proxy_channel_table_ctx = proxy_channel_thread_table_get(proxy, false))==NULL) {        
        proxy_channel_thread_table_lock_hold(false);
        mg_send_http_error(conn, 503, "proxy %s is not available\n", proxy);
        free(p_url);
        return 503;
    }

    payload = NULL;
    payload_len = 0;

    if( strcmp(method, "POST")==0
        || strcmp(method, "PUT")==0
        || strcmp(method, "PATCH")==0
    ) {
    ////header's content-type
        content_type = NULL;
        for(i=0; i<ri->num_headers; i++) {
            if(strcasecmp(ri->http_headers[i].name, "Content-Type")==0) {
                content_type = ri->http_headers[i].value;
                break;
            }
        }

        if(content_type == NULL) {
            mg_send_http_error(conn, 400, "%s\n", "header does not contain Content-Type");
            free(p_url);
            return 400;
        }

        if( content_type != strstr(content_type, "application/json") ) {
            mg_send_http_error(conn, 400, "Content-Type %s is not application/json\n", content_type);
            free(p_url);
            return 400;
        }

    ////payload
        while( (read_len = mg_read(conn, buffer, sizeof(buffer))) > 0 ) {
            if(payload==NULL) {
                write_offset = 0;
                payload_len = read_len;
                payload = (char*)malloc(payload_len + 1);                       
            } else {
                write_offset = payload_len;
                payload_len += read_len;
                payload = (char*)realloc(payload, payload_len + 1);
            }
            memcpy(payload + write_offset, buffer, read_len);
            payload[payload_len] = 0; 
        }
    } else if(strcmp(method, "GET")==0) {        
        param = ri->query_string != NULL ? strdup(ri->query_string) : NULL;
        if(param!=NULL) {
            param_json = cJSON_CreateObject();
            run = param;
            while( (p = strchr(run, '&')) != NULL ) {
                *p = 0;  
            ////parse param
                rest_param_parse(run, param_json);
            ////advance param
                run = p + 1;      
            }
            ////parse the rest param
            rest_param_parse(run, param_json);
            payload = cJSON_PrintUnformatted(param_json);
            cJSON_Delete(param_json);
            free(param);
        }
    } else {
        mg_send_http_error(conn, 501, "method %s is not supported\n", method);
        free(p_url);
        return 501;
    }

    payload_json = payload!=NULL ? cJSON_Parse(payload) : NULL;
    proxy_channel_rest(proxy_channel_table_ctx->ctx, endpoint, payload_json, &rest_respond);
    
    proxy_channel_thread_table_lock_hold(false);

    if(rest_respond.code>=200 && rest_respond.code<=299) {
        mg_send_http_ok(conn, "application/json; charset=utf-8", strlen(rest_respond.json) + 1);
        mg_write(conn, rest_respond.json, strlen(rest_respond.json));
        mg_write(conn, "\n", 1);
    } else {
        mg_send_http_error(conn, rest_respond.code, "%s\n", rest_respond.err);
    }

    proxy_channel_rest_respond_destroy(&rest_respond);
    if(payload_json!=NULL) {
        free(payload_json);
    }
    if(payload!=NULL) {
        free(payload);
    }
    free(p_url);

    return rest_respond.code;
}

static void rest_param_parse(char *s, cJSON *json) {
    char *p;
    const char *key, *value = NULL;

    if(strlen(s)>0) {
        key = s;
        if( (p = strchr(s, '=')) != NULL ) {
            *p = 0;
            value = p+1;
        }
        cJSON_AddStringToObject(json, key, value!=NULL ? value : "true");
    }
}