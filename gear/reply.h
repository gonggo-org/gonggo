#ifndef _GONGGO_REPLY_H_
#define _GONGGO_REPLY_H_

#include <civetweb.h>

#include "cJSON.h"

#include "servicestatus.h"

extern cJSON* reply_base(const char *rid, enum ServiceStatus status);
extern void reply_status(struct mg_connection *conn, const char *rid, enum ServiceStatus status);

#endif //_GONGGO_REPLY_H_
