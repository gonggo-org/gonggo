#ifndef _GONGGO_SERVICE_H_
#define _GONGGO_SERVICE_H_

extern void service_route(const char *uri, const char *data, size_t datasize, const GlobalData *global_data, struct mg_connection *conn);

#endif //_GONGGO_SERVICE_H_
