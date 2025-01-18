#ifndef _GONGGO_DEFINE_H_
#define _GONGGO_DEFINE_H_

#define GONGGOLOGBUFLEN 500
#define TMSTRBUFLEN 35
#define SHMPATHBUFLEN 51
#define PROXYNAMEBUFLEN 51
#define UUIDBUFLEN 37

#define CLIENTREPLY_HEADERS_KEY "headers"
#define CLIENTREPLY_PAYLOAD_KEY "payload"
#define CLIENTREPLY_TIMEOUT_KEY "timeout"

#define CLIENTREPLY_REQUEST_STATUS_KEY "requestStatus"
#define CLIENTREPLY_SERVICE_STATUS_KEY "serviceStatus"
#define CLIENTREPLY_SERVICE_RID_KEY "rid"
#define CLIENTREPLY_SERVICE_PROXY_KEY "proxy"

/*SERVICE_*** must be the same as proxy */
#define SERVICE_HEADERS_KEY "headers"
#define SERVICE_PAYLOAD_KEY "payload"
#define SERVICE_PROXY_KEY "proxy"
#define SERVICE_SERVICE_KEY "service"
#define SERVICE_RID_KEY "rid"

/*GONGGOSERVICE_REQUEST_DROP must be the same as proxy*/
#define GONGGOSERVICE_REQUEST_DROP "gonggorequestdrop"

#endif //_GONGGO_DEFINE_H_