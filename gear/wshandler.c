#include <civetweb.h>

#include "globaldata.h"
#include "log.h"
#include "service.h"
#include "requesttable.h"

/*Handler for new websocket connections.*/
int ws_connect_handler(const struct mg_connection *conn, void *user_data) {
    GlobalData *gd = (GlobalData*)user_data;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log(gd->log_ctx, "INFO",
        "new connection %s with request uri %s",
        ri->remote_addr,
        ri->request_uri);
    return 0;
}

/* Handler indicating the client is ready to receive data. */
void ws_ready_handler(struct mg_connection *conn, void *user_data) {
    GlobalData *gd = (GlobalData*)user_data;
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log(gd->log_ctx, "INFO",
        "client ready %s", ri->remote_addr);
    return;
}

/* Handler indicating the client sent data to the server. */
int ws_data_handler(struct mg_connection *conn, int opcode, char *data,
    size_t datasize, void *user_data) {

    const struct mg_request_info *ri = mg_get_request_info(conn);

    GlobalData *gd = (GlobalData*)user_data;
    gonggo_log(gd->log_ctx, "INFO",
        "data sent from %s", ri->remote_addr);

    int oc = opcode & 0xf;
    if( oc == MG_WEBSOCKET_OPCODE_TEXT )
        service_route(ri->request_uri, data, datasize, gd, conn);

    return 1;
}

/* Handler indicating the connection to the client is closing. */
void ws_close_handler(const struct mg_connection *conn, void *user_data) {
    GlobalData *gd;

    gd = (GlobalData*)user_data;
    request_table_conn_remove(
        gd->request_table, gd->mtx_request_table,
        gd->proxy_thread_table, gd->mtx_thread_table,
        conn, gd->log_ctx);
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log(gd->log_ctx, "INFO", "disconnect %s", ri->remote_addr);

    return;
}
