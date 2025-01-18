#include "strarr.h"

static void str_arr_create(GHashTable *t, StrArr *sa, bool from_key);

void str_arr_create_keys(GHashTable *t, StrArr *sa) {
    str_arr_create(t, sa, true);
}

void str_arr_create_values(GHashTable *t, StrArr *sa) {
    str_arr_create(t, sa, false);
}

static void str_arr_create(GHashTable *t, StrArr *sa, bool from_key) {
    GHashTableIter iter;
    gpointer key, value;
    int i = 0;

    sa->arr = NULL;
    sa->len = t!=NULL ? g_hash_table_size(t) : 0;
    if(sa->len > 0) {
        sa->arr = (char**)malloc(sa->len * sizeof(char*));
        g_hash_table_iter_init(&iter, t);
        while (g_hash_table_iter_next(&iter, &key, &value)) {
            sa->arr[i++] = strdup((const char*)(from_key ? key : value));
        }
    }
}

void str_arr_destroy(StrArr *sa, bool free_char) {
    unsigned int i;

    for(i=0; free_char && sa->arr!=NULL && i<sa->len; i++) {
        free(sa->arr[i]);
    }
    if(sa->arr!=NULL) {
        free(sa->arr);
        sa->arr = NULL;
    }
    sa->len = 0;
}