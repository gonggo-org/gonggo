#ifndef _GONGGO_CONVAR_H_
#define _GONGGO_CONVAR_H_

#include <stdbool.h>

#define CONF_PIDFILE "pidfile"
#define CONF_LOGPATH "logpath"
#define CONF_SSLCERT "sslcert"
#define CONF_SSLCERTCHAIN "sslcertchain"
#define CONF_PORT "port"
#define CONF_THREADS "threads"
#define CONF_GONGGO "gonggo"
#define CONF_DBPATH "dbpath"
#define CONF_RESPONDDRAINOVERDUE "responddrainoverdue"
#define CONF_RESPONDDRAINPERIOD "responddrainperiod"
#define CONF_RESPONDQUERYSIZE "respondquerysize"
#define CONF_CLIENTTIMEOUTPERIOD "clienttimeoutperiod"
#define CONF_PING "pingms"

typedef struct ConfVar
{
	char *name;
	char *value;
	struct ConfVar *next;
} ConfVar;

extern ConfVar* confvar_validate(const char *file, char** error);
extern ConfVar* confvar_create(const char *file);
extern void confvar_destroy(ConfVar *head);
extern size_t confvar_absent(const ConfVar *head, char *absent_key, size_t buflen);
extern const char* confvar_value(const ConfVar *head, const char *key);
extern bool confvar_long(const ConfVar *head, const char *key, long *value);
extern bool confvar_float(const ConfVar *head, const char *key, float *value);
extern bool confvar_uint(const ConfVar *head, const char *key, unsigned int *value);

#endif //_GONGGO_CONVAR_H_
