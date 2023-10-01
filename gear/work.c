#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>
#include <unistd.h>
#include <civetweb/civetweb.h>
#include <sys/mman.h>
#include <sys/stat.h>        /* For mode constants */
#include <fcntl.h>           /* For O_* constants */

#include "confvar.h"
#include "log.h"
#include "error.h"
#include "wshandler.h"
#include "proxyactivator.h"
#include "heartbeat.h"
#include "globaldata.h"
#include "requesttable.h"
#include "proxythreadtable.h"
#include "db.h"
#include "respondretain.h"

volatile bool gonggo_exit = false;

static void handler(int signal, siginfo_t *info, void *context);
static void cleanup(LogContext *log_ctx,
    pthread_mutex_t *mtx_thread_table, pthread_mutex_t *mtx_db, pthread_mutex_t *mtx_request_table,
    ProxyActivationThreadData *proxy_activation_thread_data,
    HeartbeatThreadData *heartbeat_thread_data,
    RespondRetainThreadData *respond_retain_thread_data,
    struct mg_context *http_ctx,
    GHashTable *request_table, GHashTable *proxy_thread_table,
    char *gonggo_path, char *gonggo_heartbeat);

int work(pid_t pid, const ConfVar *cv_head) {
    char buff[GONGGOLOGBUFLEN];
    const char *sslcert, *sslcertchain, *s;
    struct sigaction action;
    pthread_mutexattr_t mtx_attr;
    pthread_mutex_t mtx_log, mtx_thread_table, mtx_db, mtx_request_table;
    pthread_attr_t attr;
    pthread_t t_proxy_activation_listener, t_heartbeat, t_respond_retain;
    LogContext log_context;
    HeartbeatThreadData heartbeat_thread_data;
    ProxyActivationThreadData proxy_acctivation_thread_data;
    RespondRetainThreadData respond_retain_thread_data;
    long heartbeat_period, respond_retain_overdue, respond_retain_period, respond_query_size;
    float heartbeat_timeout_factor;
    struct mg_context *http_ctx;
    GHashTable *request_table; //map rid to client conn
    GHashTable *proxy_thread_table; //map proxy-name to thread id pointer
    GlobalData global_data;
    const char *gonggo_name;
    char *gonggo_path, *gonggo_heartbeat;
    DbContext db_ctx;

    pthread_mutexattr_init(&mtx_attr);
    pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
    pthread_mutex_init(&mtx_log, &mtx_attr);
    pthread_mutex_init(&mtx_thread_table, &mtx_attr);
    pthread_mutex_init(&mtx_db, &mtx_attr);
    pthread_mutex_init(&mtx_request_table, &mtx_attr);
    pthread_mutexattr_destroy(&mtx_attr);

    gonggo_name = confvar_value(cv_head, CONF_GONGGO);

    log_context_init(&log_context, pid, confvar_value(cv_head, CONF_LOGPATH), gonggo_name, &mtx_log);

    db_context_setup(&db_ctx, gonggo_name, confvar_value(cv_head, CONF_DBPATH), &mtx_db);
    db_ensure(&db_ctx, &log_context);

    sslcert = confvar_value(cv_head, CONF_SSLCERT);
    sslcertchain = confvar_value(cv_head, CONF_SSLCERTCHAIN);

    confvar_long(cv_head, CONF_HEARTBEATPERIOD, &heartbeat_period);
    confvar_float(cv_head, CONF_HEARTBEATTIMEOUT, &heartbeat_timeout_factor);
    confvar_long(cv_head, CONF_RESPONDRETAINOVERDUE, &respond_retain_overdue);
    confvar_long(cv_head, CONF_RESPONDRETAINPERIOD, &respond_retain_period);
    confvar_long(cv_head, CONF_RESPONDQUERYSIZE, &respond_query_size);

    memset(&action, 0, sizeof(action));
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;
    if(sigaction(SIGTERM, &action, NULL)==-1) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log(&log_context, "ERROR", "sigaction failed %s", buff);
        pthread_mutex_destroy(&mtx_thread_table);
        log_context_destroy(&log_context);
        return ERROR_TERMHANDLER;
    }

    http_ctx = NULL;

    request_table = request_table_create();
    proxy_thread_table = proxy_thread_table_create();

	mg_init_library(sslcert==NULL ? 0 : 2);

    int idx = 0;
    const char *options[] = {
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL
    };

    if( sslcert!=NULL ) {
        options[idx++] = "ssl_certificate";
        options[idx++] = sslcert;
    }

	if( sslcertchain!=NULL ) {
        options[idx++] = "ssl_certificate_chain";
        options[idx++] = sslcertchain;
	}

    options[idx++] = "listening_ports";
    options[idx++] = confvar_value(cv_head, CONF_PORT);

    s = confvar_value(cv_head, CONF_THREADS);
    if( s!=NULL ) {
        options[idx++] = "num_threads";
        options[idx++] = s;
    }

    options[idx++] = "error_log_file";
    options[idx++] = "error.log";

    asprintf(&gonggo_path, "/%s", gonggo_name);
    asprintf(&gonggo_heartbeat, "/%s_heartbeat", gonggo_name);

    global_data_setup(&global_data, &log_context,
        request_table, &mtx_request_table,
        proxy_thread_table, &mtx_thread_table,
        gonggo_path, &db_ctx, respond_query_size);

    struct mg_callbacks callbacks = {0};
    struct mg_init_data mg_start_init_data = {0};
    mg_start_init_data.callbacks = &callbacks;
    mg_start_init_data.user_data = &global_data;
    mg_start_init_data.configuration_options = options;

    struct mg_error_data mg_start_error_data = {0};
    char errtxtbuf[256] = {0};
    mg_start_error_data.text = errtxtbuf;
    mg_start_error_data.text_buffer_size = sizeof(errtxtbuf);

    proxy_activation_thread_data_init( &proxy_acctivation_thread_data );
    heartbeat_thread_data_init( &heartbeat_thread_data );
    respondretain_thread_data_init( &respond_retain_thread_data );

    http_ctx = mg_start2(&mg_start_init_data, &mg_start_error_data);
    if (http_ctx == NULL) {
        gonggo_log(&log_context, "ERROR", "cannot start server, %s", errtxtbuf);
        cleanup(&log_context, &mtx_thread_table, &mtx_db, &mtx_request_table,
            &proxy_acctivation_thread_data, &heartbeat_thread_data, &respond_retain_thread_data,
            http_ctx,
            request_table, proxy_thread_table, gonggo_path, gonggo_heartbeat);
        return ERROR_START;
    }

    /* Register the websocket callback functions. */
    mg_set_websocket_handler(
        http_ctx,
        gonggo_path,
        ws_connect_handler, //mg_websocket_connect_handler connect_handler,
        ws_ready_handler, //mg_websocket_ready_handler ready_handler,
        ws_data_handler, //mg_websocket_data_handler data_handler,
        ws_close_handler, //mg_websocket_close_handler close_handler,
        &global_data);

    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    proxy_activation_thread_data_setup(&proxy_acctivation_thread_data,
        &log_context, gonggo_path,
        request_table, &mtx_request_table,
        proxy_thread_table, &mtx_thread_table,
        heartbeat_period, &db_ctx);
    if( pthread_create( &t_proxy_activation_listener, &attr, proxy_activation_listener, &proxy_acctivation_thread_data ) != 0 ) {
        pthread_attr_destroy(&attr);//destroy thread-attribute
        gonggo_log(&log_context, "ERROR", "cannot start server, %s", "activation listener thread creation is failed");
        cleanup(&log_context, &mtx_thread_table, &mtx_db, &mtx_request_table,
            &proxy_acctivation_thread_data, &heartbeat_thread_data, &respond_retain_thread_data,
            http_ctx,
            request_table, proxy_thread_table, gonggo_path, gonggo_heartbeat);
        return ERROR_START;
    }
    while( !proxy_acctivation_thread_data.started )
        usleep(1000);

    heartbeat_thread_data_setup(&heartbeat_thread_data, &log_context, heartbeat_period, heartbeat_timeout_factor,
        gonggo_heartbeat);
    if( pthread_create( &t_heartbeat, &attr, heartbeat, &heartbeat_thread_data ) != 0 ) {
        pthread_attr_destroy(&attr);//destroy thread-attribute
        gonggo_log(&log_context, "ERROR", "cannot start server, %s", "heartbeat thread creation is failed");
        gonggo_exit = true;
        proxy_activation_thread_stop(&proxy_acctivation_thread_data, t_proxy_activation_listener);
        cleanup(&log_context, &mtx_thread_table, &mtx_db, &mtx_request_table,
            &proxy_acctivation_thread_data, &heartbeat_thread_data, &respond_retain_thread_data,
            http_ctx,
            request_table, proxy_thread_table, gonggo_path, gonggo_heartbeat);
        return ERROR_START;
    }
    while( !heartbeat_thread_data.started )
        usleep(1000);

    respondretain_thread_data_setup(&respond_retain_thread_data, &log_context, respond_retain_overdue, respond_retain_period, &db_ctx);
    if( pthread_create( &t_respond_retain, &attr, respondretain, &respond_retain_thread_data ) != 0 ) {
        pthread_attr_destroy(&attr);//destroy thread-attribute
        gonggo_log(&log_context, "ERROR", "cannot start server, %s", "respond retain thread creation is failed");
        gonggo_exit = true;
        proxy_activation_thread_stop(&proxy_acctivation_thread_data, t_proxy_activation_listener);
        heartbeat_thread_stop(&heartbeat_thread_data, t_heartbeat);
        cleanup(&log_context, &mtx_thread_table, &mtx_db, &mtx_request_table,
            &proxy_acctivation_thread_data, &heartbeat_thread_data, &respond_retain_thread_data,
            http_ctx,
            request_table, proxy_thread_table, gonggo_path, gonggo_heartbeat);
        return ERROR_START;
    }
    while( !respond_retain_thread_data.started )
        usleep(1000);

    pthread_attr_destroy(&attr);//destroy thread-attribute

    gonggo_log(&log_context, "INFO", "gonggo server started");

    while( !gonggo_exit )
        pause();

    gonggo_log(&log_context, "INFO", "gonggo server is stopping");

    proxy_activation_thread_stop(&proxy_acctivation_thread_data, t_proxy_activation_listener);
    heartbeat_thread_stop(&heartbeat_thread_data, t_heartbeat);
    respondretain_thread_stop(&respond_retain_thread_data, t_respond_retain);

    gonggo_log(&log_context, "INFO", "gonggo server stopped");

    cleanup(&log_context, &mtx_thread_table, &mtx_db, &mtx_request_table,
        &proxy_acctivation_thread_data, &heartbeat_thread_data, &respond_retain_thread_data,
        http_ctx,
        request_table, proxy_thread_table, gonggo_path, gonggo_heartbeat);

    gonggo_log(&log_context, "INFO", "gonggo server resources is cleaned up");

    return 0;
}

static void handler(int signal, siginfo_t *info, void *context) {
    if(signal==SIGTERM)
        gonggo_exit = true;
}

static void cleanup(LogContext *log_ctx,
    pthread_mutex_t *mtx_thread_table, pthread_mutex_t *mtx_db, pthread_mutex_t *mtx_request_table,
    ProxyActivationThreadData *proxy_activation_thread_data,
    HeartbeatThreadData *heartbeat_thread_data,
    RespondRetainThreadData *respond_retain_thread_data,
    struct mg_context *http_ctx,
    GHashTable *request_table, GHashTable *proxy_thread_table,
    char *gonggo_path, char *gonggo_heartbeat)
{
    gonggo_log(log_ctx, "INFO", "clean up");
    log_context_destroy( log_ctx );
    pthread_mutex_destroy(mtx_thread_table);
    pthread_mutex_destroy(mtx_db);
    pthread_mutex_destroy(mtx_request_table);
    proxy_activation_thread_data_destroy(proxy_activation_thread_data);
    heartbeat_thread_data_destroy(heartbeat_thread_data);
    respondretain_thread_data_destroy(respond_retain_thread_data);
    if( http_ctx!=NULL )
        mg_stop( http_ctx );//stop server, disconnect all clients
    request_table_destroy(request_table);
    proxy_thread_table_destroy(proxy_thread_table);
    if(gonggo_path!=NULL)
        free(gonggo_path);
    if(gonggo_heartbeat!=NULL)
        free(gonggo_heartbeat);
    mg_exit_library();//deinitialize CivetWeb library
}
