#include <civetweb.h>
#include <glib.h>

#include "globaldata.h"
#include "log.h"
#include "clientservice.h"
#include "clientconnectiontable.h"
#include "clientrequesttable.h"

/*Handler for new websocket connections.*/
int ws_connect_handler(const struct mg_connection *conn, void *user_data) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log("INFO", "new connection %s with request uri %s",
        ri->remote_addr,
        ri->request_uri);
    return 0;
}

/* Handler indicating the client is ready to receive data. */
void ws_ready_handler(struct mg_connection *conn, void *user_data) {
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log("INFO", "client ready %s", ri->remote_addr);
    return;
}

/* Handler indicating the client sent data to the server. */
int ws_data_handler(struct mg_connection *conn, int opcode, char *data,
    size_t datasize, void *user_data) {

    const struct mg_request_info *ri = mg_get_request_info(conn);

    gonggo_log("INFO", "data sent from %s", ri->remote_addr);

    int oc = opcode & 0xf;
    if( oc == MG_WEBSOCKET_OPCODE_TEXT ) {
        client_service_route(ri->remote_addr, ri->request_uri, data, datasize, conn);
    }

    return 1;
}

/* Handler indicating the connection to the client is closing. */
void ws_close_handler(const struct mg_connection *conn, void *user_data) {
    client_service_drop_conn(conn);
    const struct mg_request_info *ri = mg_get_request_info(conn);
    gonggo_log("INFO", "disconnect %s", ri->remote_addr);

    return;
}