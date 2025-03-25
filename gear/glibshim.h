#ifndef _GLIB2SHIM_H_
#define _GLIB2SHIM_H_

#ifdef GLIBSHIM

#include <glib.h>

#ifndef g_ptr_array_copy
extern GPtrArray* gonggo_ptr_array_copy (GPtrArray* array, GCopyFunc func, gpointer user_data);
#define g_ptr_array_copy gonggo_ptr_array_copy
#endif //g_ptr_array_copy

#endif //GLIBSHIM

#endif //_GLIB2SHIM_H_