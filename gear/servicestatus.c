#include "servicestatus.h"

/*     = 0,
    SERVICE_STATUS_REQUEST_ANSWERED = 1,*/
const char* service_status(enum ServiceStatus status) {
    switch(status) {
        case SERVICE_STATUS_REQUEST_ACKNOWLEDGED:
            return "Request is acknowledged";
            break;
        case SERVICE_STATUS_REQUEST_ANSWERED:
            return "Request is answered";
            break;
        case SERVICE_STATUS_INTERNAL_ERROR:
            return "Internal dispatcher error";
            break;
        case SERVICE_STATUS_INVALID_PAYLOAD:
            return "Request has invalid payload";
            break;
        case SERVICE_STATUS_RID_OVERLENGTH:
            return "RID length exceeds limit";
            break;
        case SERVICE_STATUS_PATH_OVERLENGTH:
            return "Path length exceeds limit";
            break;
        case SERVICE_STATUS_TASK_EMPTY:
            return "Task is empty";
            break;
        case SERVICE_STATUS_TASK_OVERLENGTH:
            return "Task length exceeds limit";
            break;
        case SERVICE_STATUS_PROXY_NOT_AVAILABLE:
            return "Proxy is not available";
            break;
        case SERVICE_STATUS_PROXY_NOT_ACCESSIBLE:
            return "Proxy is not accessible";
            break;
        case SERVICE_STATUS_PROXY_BUSY:
            return "Proxy is busy";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_DIED:
            return "Proxy is died";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_OPEN_FAILED:
            return "Proxy Channel opening is failed";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_ACCESS_FAILED:
            return "Proxy Channel accessing is failed";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_BUSY:
            return "Proxy Channel request is busy";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_INVALID_TASK:
            return "Proxy Channel request has invalid task";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_INVALID_PAYLOAD:
            return "Proxy Channel request has invalid payload";
            break;
        case SERVICE_STATUS_PROXY_CHANNEL_INVALID_STATE:
            return "Proxy Channel request has invalid state";
            break;
        default:
            break;
    }
    return "No description";
}
