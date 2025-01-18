#ifndef _STRARR_H_
#define _STRARR_H_

#include <stdbool.h>
#include <glib.h>

typedef struct StrArr {
    char **arr;
    unsigned int len;
} StrArr;

extern void str_arr_create_keys(GHashTable *t, StrArr *sa);
extern void str_arr_create_values(GHashTable *t, StrArr *sa);
extern void str_arr_destroy(StrArr *sa, bool free_char);

#endif //_STRARR_H_