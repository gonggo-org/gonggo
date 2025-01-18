#include <stdbool.h>
#include <glib.h>

#include "strarr.h"
#include "clientproxynametable.h"

#include "log.h"
#include "util.h"

static pthread_mutex_t client_proxyname_table_lock;
//map proxy-name to ClientProxynameTableValue
static GHashTable *client_proxyname_table = NULL;
static void client_proxyname_table_value_destroy(ClientProxynameTableValue *p);

void client_proxyname_table_create(void) {
    pthread_mutexattr_t mtx_attr;

    if(client_proxyname_table==NULL) {
        pthread_mutexattr_init(&mtx_attr);
        pthread_mutexattr_setpshared(&mtx_attr, PTHREAD_PROCESS_PRIVATE);
        pthread_mutexattr_settype(&mtx_attr, PTHREAD_MUTEX_NORMAL);
        pthread_mutex_init(&client_proxyname_table_lock, &mtx_attr);
        pthread_mutexattr_destroy(&mtx_attr);

        client_proxyname_table = g_hash_table_new_full(g_str_hash, g_str_equal, (GDestroyNotify)free, 
            (GDestroyNotify)client_proxyname_table_value_destroy);
    }
}

GHashTable *client_proxyname_table_similar(void) {
    return client_proxyname_table !=NULL ? g_hash_table_new_similar(client_proxyname_table) : NULL;
}

void client_proxyname_table_destroy(void) {
    if(client_proxyname_table!=NULL) {
        g_hash_table_destroy(client_proxyname_table);
        client_proxyname_table = NULL;
        pthread_mutex_destroy(&client_proxyname_table_lock);
    }
}

void client_proxyname_table_set(const char* proxy_name, const char* request_uuid, GHashTable *other_table_nolock) {
    ClientProxynameTableValue *value;
    bool exist = false, exist_default = false;
    GHashTable *table;

    table = other_table_nolock!=NULL ? other_table_nolock : client_proxyname_table;
    if(table==client_proxyname_table) {
        pthread_mutex_lock(&client_proxyname_table_lock);
    }
    value = (ClientProxynameTableValue*)g_hash_table_lookup(table, proxy_name);
    if(value==NULL) {
        value = (ClientProxynameTableValue*)malloc(sizeof(ClientProxynameTableValue));
        value->request_uuids = g_ptr_array_new_full(1, (GDestroyNotify)free);
        value->sents =  g_array_new(FALSE, FALSE, sizeof(bool));
        g_hash_table_insert(table, strdup(proxy_name), value);
    } else {
        exist = g_ptr_array_find_with_equal_func(value->request_uuids, request_uuid, (GEqualFunc)str_equal, NULL);
    }
    if(!exist) {
        g_ptr_array_add(value->request_uuids, strdup(request_uuid));
        g_array_append_val(value->sents, exist_default);
    }
    if(table==client_proxyname_table) {
        pthread_mutex_unlock(&client_proxyname_table_lock);
    }
}

void client_proxyname_table_request_sent(const char *proxy_name, const char *request_uuid) {
    ClientProxynameTableValue *value;
    guint idx;

    pthread_mutex_lock(&client_proxyname_table_lock);
    value = (ClientProxynameTableValue*)g_hash_table_lookup(client_proxyname_table, proxy_name);
    if(value!=NULL && g_ptr_array_find_with_equal_func(value->request_uuids, request_uuid, (GEqualFunc)str_equal, &idx)) {
        g_array_index(value->sents, bool, idx) = true;
    }
    pthread_mutex_unlock(&client_proxyname_table_lock);
}

void client_proxyname_table_request_unsent(const char *proxy_name) {
    ClientProxynameTableValue *value;
    guint i;

    pthread_mutex_lock(&client_proxyname_table_lock);
    if((value = (ClientProxynameTableValue*)g_hash_table_lookup(client_proxyname_table, proxy_name)) != NULL) {
        for(i=0; i<value->sents->len; i++) {
            g_array_index(value->sents, bool, i) = false;
        }
    }
    pthread_mutex_unlock(&client_proxyname_table_lock);
}

bool client_proxyname_table_drop(const char* proxy_name, const char* request_uuid) {
    ClientProxynameTableValue *value;
    guint idx;
    bool found = false;

    pthread_mutex_lock(&client_proxyname_table_lock);
    if((value = (ClientProxynameTableValue*)g_hash_table_lookup(client_proxyname_table, proxy_name)) != NULL) {
        if((found = g_ptr_array_find_with_equal_func(value->request_uuids, request_uuid, (GEqualFunc)str_equal, &idx))) {
            g_ptr_array_remove_index(value->request_uuids, idx);
            g_array_remove_index(value->sents, idx);
            if(value->request_uuids->len<1) {
                g_hash_table_remove(client_proxyname_table, proxy_name);
            }
        }
    }
    pthread_mutex_unlock(&client_proxyname_table_lock);
    return found;
}

void client_proxyname_table_drop_all(const char* request_uuid) {////search in all proxy_names
    StrArr sa;
    unsigned int i;

    pthread_mutex_lock(&client_proxyname_table_lock);
    str_arr_create_keys(client_proxyname_table, &sa);
    pthread_mutex_unlock(&client_proxyname_table_lock);
    for(i=0; i<sa.len; i++) {
        if(client_proxyname_table_drop(sa.arr[i]/*proxy_name*/, request_uuid)){
            break;
        }
    }
    str_arr_destroy(&sa, true);
}

GPtrArray* client_proxyname_table_request_dup(const char* proxy_name, bool not_sent_only) {
    ClientProxynameTableValue *value;
    GPtrArray *p, *p2;
    guint i;


    pthread_mutex_lock(&client_proxyname_table_lock);
    value = (ClientProxynameTableValue*)g_hash_table_lookup(client_proxyname_table, proxy_name);
    p = value!=NULL ? value->request_uuids : NULL;
    if(p!=NULL) {
        if(not_sent_only) {
            p2 = g_ptr_array_new_full(1, (GDestroyNotify)free);
            for(i=0; i<value->sents->len; i++) {
                if(!g_array_index(value->sents, bool, i)) {
                    g_ptr_array_add(p2, strdup(g_ptr_array_index(p, i)));
                }
            }
            p = p2;
        } else {
            p = g_ptr_array_copy(p, (GCopyFunc)str_dup, NULL);
        }
    }    
    pthread_mutex_unlock(&client_proxyname_table_lock);

    return p;    
}

char *client_proxyname_table_proxy_name_dup(const char *request_uuid) {
    char *proxy_name = NULL, *t;
    GHashTableIter iter;
    ClientProxynameTableValue *value;

    pthread_mutex_lock(&client_proxyname_table_lock);
    g_hash_table_iter_init(&iter, client_proxyname_table);
    while (g_hash_table_iter_next(&iter, (gpointer*)&t, (gpointer*)&value)) {
        if(g_ptr_array_find_with_equal_func(value->request_uuids, request_uuid, (GEqualFunc)str_equal, NULL)) {
            proxy_name = strdup(t);
            break;
        }
    }    
    pthread_mutex_unlock(&client_proxyname_table_lock);
    return proxy_name;
}

static void client_proxyname_table_value_destroy(ClientProxynameTableValue *p) {
    if(p!=NULL) {
        if(p->request_uuids!=NULL) {
            g_ptr_array_free(p->request_uuids, true);
        }
        if(p->sents!=NULL) {
            g_array_free(p->sents, true);
        }
        free(p);
    }
}