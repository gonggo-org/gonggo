#include "globaldata.h"

void global_data_setup(GlobalData *gd, const LogContext *log_ctx,
    GHashTable *request_table, pthread_mutex_t *mtx_request_table,
    GHashTable *proxy_thread_table, pthread_mutex_t *mtx_thread_table,
    const char *activation_path, const DbContext *db_ctx, int respond_query_size)
{
    gd->log_ctx = log_ctx;
    gd->request_table = request_table;
    gd->mtx_request_table = mtx_request_table;
    gd->proxy_thread_table = proxy_thread_table;
    gd->mtx_thread_table = mtx_thread_table;
    gd->activation_path = activation_path;
    gd->db_ctx = db_ctx;
    gd->respond_query_size = respond_query_size;
}
