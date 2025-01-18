#ifndef _SERVICESTATUS_H_
#define _SERVICESTATUS_H_

enum ServiceStatus {
    REQUEST_PARSE_STATUS_ACKNOWLEDGED = 1,    
    REQUEST_PARSE_STATUS_HEADERS_MISSING = 2,    
    REQUEST_PARSE_STATUS_RID_MISSING = 3,
    REQUEST_PARSE_STATUS_RID_OVERLENGTH = 4,
    REQUEST_PARSE_STATUS_SERVICE_MISSING = 5,
    REQUEST_PARSE_STATUS_SERVICE_RESERVED = 6,
    
    REQUEST_PARSE_STATUS_PAYLOAD_MISSING = 7,
    REQUEST_PARSE_STATUS_PAYLOAD_INVALID = 8,
    REQUEST_PARSE_STATUS_GONGGO_SERVICE_INVALID = 9,
    REQUEST_PARSE_STATUS_TIMEOUT_MISSING = 10,
    REQUEST_PARSE_STATUS_TIMEOUT_INVALID = 11,
    

    REQUEST_STATUS_ANSWERED = 20,
    REQUEST_STATUS_PROXY_ALIVE_NOTIFICATION = 21,
    REQUEST_STATUS_EXPIRED = 22,

    PROXY_STATUS_START = 30,
    PROXY_STATUS_TERMINATED = 31
};

enum TestStatus {
    GONGGO_TEST_SUCCESS = 1
};

enum ResponseDumpStatus {
    GONGGO_RESPONSE_DUMP_SUCCESS = 1,
    GONGGO_RESPONSE_DUMP_DB_ERROR = 2
};

/*ProxyServiceStatus must match proxy ProxyServiceStatus*/ 
enum ProxyServiceStatus {
    PROXYSERVICESTATUS_ALIVE_START = 1,
    PROXYSERVICESTATUS_ALIVE_TERMINATION = 2,
    PROXYSERVICESTATUS_MULTIRESPOND_CLEAR_SUCCESS = 3,
    PROXYSERVICESTATUS_MULTIRESPOND_CLEAR_PAYLOAD_MISSING = 4,
    PROXYSERVICESTATUS_MULTIRESPOND_CLEAR_PAYLOAD_RID_MISSING = 5,
    PROXYSERVICESTATUS_MULTIRESPOND_CLEAR_PAYLOAD_RID_INVALID = 6
};

#endif //_SERVICESTATUS_H_