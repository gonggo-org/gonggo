#ifndef _GONGGO_WSHANDLER_H_
#define _GONGGO_WSHANDLER_H_

/*Handler for new websocket connections.*/
extern int ws_connect_handler(const struct mg_connection *conn, void *user_data);

/* Handler indicating the client is ready to receive data. */
extern void ws_ready_handler(struct mg_connection *conn, void *user_data);

/* Handler indicating the client sent data to the server. */
extern int ws_data_handler(struct mg_connection *conn, int opcode, char *data,
    size_t datasize, void *user_data);

/* Handler indicating the connection to the client is closing. */
extern void ws_close_handler(const struct mg_connection *conn, void *user_data);

#endif //_GONGGO_WSHANDLER_H_