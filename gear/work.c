#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <civetweb/civetweb.h>

#include "confvar.h"
#include "log.h"
#include "error.h"
#include "globaldata.h"
#include "wshandler.h"
#include "proxychannelthreadtable.h"
#include "proxysubscribethreadtable.h"
#include "proxyalivethreadtable.h"
#include "clientrequesttable.h"
#include "clientconnectiontable.h"
#include "clientservicetable.h"
#include "clientproxynametable.h"
#include "db.h"
#include "dbresponddrain.h"
#include "proxyactivator.h"
#include "clienttimeout.h"
#include "alivemutex.h"
#include "proxyterminator.h"
#include "proxyterminatearray.h"
#include "gonggouuid.h"

const char *gonggo_name = NULL;
volatile bool gonggo_exit = false;

static bool mg_library_initialized = false;
static void handler(int signal, siginfo_t *info, void *context);
static void clean_up(struct mg_context *http_ctx);
static void threads_stop(pthread_t t_proxyactivator, pthread_t t_responddrain, pthread_t t_clienttimeout, pthread_t t_proxy_terminator);

int work(pid_t pid, const ConfVar *cv_head) 
{
	const char *sslcert, *s;
	struct sigaction action;	
	char buff[GONGGOLOGBUFLEN];	
	long respond_drain_overdue, respond_drain_period, clienttimeout_period, ping;
	pthread_t t_proxy_activator, t_respond_drain, t_client_timeout, t_proxy_terminator;
	pthread_attr_t thread_attr;

////web server:BEGIN	
	char mg_errtxtbuf[256] = {0};	
	struct mg_callbacks callbacks = {0};
    struct mg_init_data mg_start_init_data = {0};
	struct mg_error_data mg_start_error_data = {0};
	const char* mg_options[] = {
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
        NULL, NULL,
		NULL, NULL,
        NULL, NULL
    };	
	struct mg_context *http_ctx = NULL;
////web server:END

	gonggo_name = confvar_value(cv_head, CONF_GONGGO);

	gonggo_log_context_init(pid, confvar_value(cv_head, CONF_LOGPATH));
	gonggo_uuid_init();

	proxy_thread_killer_init();
	db_context_init(gonggo_name, confvar_value(cv_head, CONF_DBPATH));
	
	if(!db_ensure()) {
		gonggo_log("ERROR", "database initialization failed");
		clean_up(http_ctx);		
		return ERROR_DBINIT;		
	}

	memset(&action, 0, sizeof(action));
    action.sa_flags = SA_SIGINFO;
    action.sa_sigaction = handler;
    if(sigaction(SIGTERM, &action, NULL)==-1) {
        strerror_r(errno, buff, GONGGOLOGBUFLEN);
        gonggo_log("ERROR", "sigaction failed %s", buff);
		clean_up(http_ctx);
        return ERROR_TERMHANDLER;
    }

////tables:BEGIN
	proxy_subscribe_thread_table_create();
	proxy_channel_thread_table_create();
	proxy_alive_thread_table_create();
	client_request_table_create();
	client_connection_table_create();
	client_service_table_create();
	client_proxyname_table_create();
	proxy_terminate_array_create();
////tables:END
	
	if(!alive_mutex_create(true)) {
		clean_up(http_ctx);
		return ERROR_START;
	}

////thread context initialization:BEGIN	
	if(!proxy_activator_context_init()){		
		clean_up(http_ctx);
		return ERROR_START;
	}

	proxy_terminator_context_init();

	confvar_long(cv_head, CONF_RESPONDDRAINOVERDUE, &respond_drain_overdue);
    confvar_long(cv_head, CONF_RESPONDDRAINPERIOD, &respond_drain_period);
	db_respond_drain_context_init(respond_drain_overdue, respond_drain_period);

	confvar_long(cv_head, CONF_CLIENTTIMEOUTPERIOD, &clienttimeout_period);
	client_timeout_context_init(clienttimeout_period);	
////thread context initialization:END

    pthread_attr_init(&thread_attr);
    pthread_attr_setdetachstate(&thread_attr, PTHREAD_CREATE_JOINABLE);
	do {
		if(pthread_create(&t_respond_drain, &thread_attr, db_respond_drain, NULL)!=0) {
			gonggo_log("ERROR", "cannot start server, %s", "db respond drain thread creation is failed");	
			break;
		}
		db_respond_drain_waitfor_started();

		if(pthread_create(&t_client_timeout, &thread_attr, client_timeout, NULL)!=0) {
			gonggo_log("ERROR", "cannot start server, %s", "client timeout thread creation is failed");
			break;	
		}
		client_timeout_waitfor_started();

		if(pthread_create(&t_proxy_terminator, &thread_attr, proxy_terminator, NULL)!=0) {
			gonggo_log("ERROR", "cannot start server, %s", "proxy terminator thread creation is failed");
			break;	
		}
		proxy_terminator_waitfor_started();

		alive_mutex_lock();

		//put proxy activation service at last to ensure the above supporting threads ready
		if(pthread_create(&t_proxy_activator, &thread_attr, proxy_activator, NULL)!=0) {
			alive_mutex_die();
			gonggo_log("ERROR", "cannot start server, %s", "proxy activator thread creation is failed");
			break;
		}
		proxy_activator_waitfor_started();
	} while(false);
	pthread_attr_destroy(&thread_attr);//destroy thread-attribute

	if(	!proxy_activator_isstarted()
		|| !db_respond_drain_isstarted()
		|| !client_timeout_isstarted()
		|| !proxy_terminator_isstarted()) 
	{
		gonggo_exit = true;
		threads_stop(t_proxy_activator, t_respond_drain, t_client_timeout, t_proxy_terminator);
		clean_up(http_ctx);
        return ERROR_START;
	}	

////webserver start:BEGIN
	sslcert = confvar_value(cv_head, CONF_SSLCERT);
	mg_init_library(sslcert==NULL ? 0 : 2);
	mg_library_initialized = true;

	int idx = 0;
    if( sslcert!=NULL ) {
        mg_options[idx++] = "ssl_certificate";
        mg_options[idx++] = sslcert;
    }

	mg_options[idx++] = "listening_ports";
    mg_options[idx++] = confvar_value(cv_head, CONF_PORT);

	s = confvar_value(cv_head, CONF_THREADS);
    if( s!=NULL ) {
        mg_options[idx++] = "num_threads";
        mg_options[idx++] = s;
    }

	confvar_long(cv_head, CONF_PING, &ping);
	mg_options[idx++] = "enable_websocket_ping_pong";
	mg_options[idx++] = ping > 0 ? "yes" : "no";
	
	if(ping>0) {
		mg_options[idx++] = "websocket_timeout_ms";
		mg_options[idx++] = confvar_value(cv_head, CONF_PING);
	}

	mg_options[idx++] = "error_log_file";
    mg_options[idx++] = "error.log";

    mg_start_init_data.callbacks = &callbacks;
    mg_start_init_data.user_data = NULL;
    mg_start_init_data.configuration_options = mg_options;

	mg_start_error_data.text = mg_errtxtbuf;
    mg_start_error_data.text_buffer_size = sizeof(mg_errtxtbuf);

	http_ctx = mg_start2(&mg_start_init_data, &mg_start_error_data);
 	if(http_ctx == NULL) {
		alive_mutex_die();
        gonggo_log("ERROR", "cannot start server, %s", mg_errtxtbuf);
		gonggo_exit = true;
		threads_stop(t_proxy_activator, t_respond_drain, t_client_timeout, t_proxy_terminator);
		clean_up(http_ctx);
        return ERROR_START;
    }	

	/* Register the websocket callback functions. */
    mg_set_websocket_handler(
        http_ctx,
        proxy_activator_get_gonggo_path(),
        ws_connect_handler, //mg_websocket_connect_handler connect_handler,
        ws_ready_handler, //mg_websocket_ready_handler ready_handler,
        ws_data_handler, //mg_websocket_data_handler data_handler,
        ws_close_handler, //mg_websocket_close_handler close_handler,
        NULL);
////webserver start:END

	gonggo_log("INFO", "gonggo server started");

	while(!gonggo_exit) {
        pause();
	}

	gonggo_log("INFO", "gonggo server is stopping");

////all proxies should be aware by checking alive_mutex_shm->alive-ness
	alive_mutex_die();

	gonggo_log("INFO", "gonggo kills all proxy threads");
	proxy_thread_kill_all();

	gonggo_log("INFO", "gonggo stops all gonggo threads");
	threads_stop(t_proxy_activator, t_respond_drain, t_client_timeout, t_proxy_terminator);

	gonggo_log("INFO", "gonggo server is stopped");	
	clean_up(http_ctx);

	return 0;
}

static void handler(int signal, siginfo_t *info, void *context) {
    if(signal==SIGTERM) {
        gonggo_exit = true;
	}
}

static void clean_up(struct mg_context *http_ctx) {
	gonggo_log_context_destroy();
	gonggo_uuid_destroy();

	proxy_subscribe_thread_table_destroy();
	proxy_channel_thread_table_destroy();
	proxy_alive_thread_table_destroy();
	client_request_table_destroy();
	client_connection_table_destroy();
	client_service_table_destroy();
	client_proxyname_table_destroy();
	proxy_terminate_array_destroy();
	alive_mutex_destroy();

	if(http_ctx!=NULL) {
		mg_stop(http_ctx);//stop server, disconnect all clients
	}

	if(mg_library_initialized) {
		mg_exit_library();
	}

	db_context_destroy();
	proxy_thread_killer_destroy();
}

static void threads_stop(pthread_t t_proxyactivator, pthread_t t_responddrain, pthread_t t_clienttimeout, pthread_t t_proxy_terminator)
{	
	if(proxy_activator_isstarted()) { 
		gonggo_log("INFO", "proxy_activator thread stopping");
		proxy_activator_stop(); 
		gonggo_log("INFO", "proxy_activator thread stopping done");
	}
	if(db_respond_drain_isstarted()) { 
		gonggo_log("INFO", "db_respond_drain thread stopping");
		db_respond_drain_stop(); 
		gonggo_log("INFO", "db_respond_drain thread stopping done");
	}
	if(client_timeout_isstarted()) { 
		gonggo_log("INFO", "client_timeout thread stopping");
		client_timeout_stop(); 
		gonggo_log("INFO", "client_timeout thread stopping done");
	}
	if(proxy_terminator_isstarted()) { 
		gonggo_log("INFO", "proxy_terminator thread stopping");
		proxy_terminator_stop(); 
		gonggo_log("INFO", "proxy_terminator thread stopping done");
	}

	if(proxy_activator_isstarted()) { 
		gonggo_log("INFO", "proxy_activator thread joining");
		pthread_join(t_proxyactivator, NULL); 
		gonggo_log("INFO", "proxy_activator thread joining done");
	}
	if(db_respond_drain_isstarted()) { 
		gonggo_log("INFO", "db_respond_drain thread joining");
		pthread_join(t_responddrain, NULL); 
		gonggo_log("INFO", "db_respond_drain thread joining done");
	}
	if(client_timeout_isstarted()) { 
		gonggo_log("INFO", "client_timeout thread joining");
		pthread_join(t_clienttimeout, NULL); 
		gonggo_log("INFO", "client_timeout thread joining done");
	}
	if(proxy_terminator_isstarted()) { 
		gonggo_log("INFO", "proxy_terminator thread joining");
		pthread_join(t_proxy_terminator, NULL); 
		gonggo_log("INFO", "proxy_terminator thread joining done");
	}

	proxy_activator_context_destroy();
	db_respond_drain_context_destroy();
	client_timeout_context_destroy();
	proxy_terminator_context_destroy();
}