#ifndef _GONGGO_RESTHANDLER_H_
#define _GONGGO_RESTHANDLER_H_

typedef struct {
    const char *gonggo_name;
} RestHandlerData;

extern int rest_handler(struct mg_connection *conn, void *cbdata);

#endif //_GONGGO_RESTHANDLER_H_