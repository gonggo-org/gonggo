# Initialize AC
AC_INIT([gonggo], [1.0.0], [abdulyadi.datatrans@gmail.com])

# Set build directory
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_SRCDIR([main/main.c])

# Initialize Automake
AM_INIT_AUTOMAKE([subdir-objects -Wall -Werror foreign])

# glib-2.0 library dependency
PKG_CHECK_MODULES([DEPS], [glib-2.0 sqlite3 uuid libcjson])

# civetweb is not available in have pkg-config
AC_ARG_WITH([civetweb],
	[AS_HELP_STRING([--with-civet=<path>], [web socket server])],
	[AS_IF([test -d "$with_civet"],
		[AC_MSG_ERROR([Specify an existing directory for civetweb])],
		[AC_MSG_NOTICE([Using civetweb from "$with_civetweb"])])],
	[]
	)
CIVETWEBHOME=
AS_IF([test "x$with_civetweb" != x],
	[AC_CHECK_LIB([civetweb], [mg_init_library],
		[AC_SUBST([CIVETWEBHOME], ["$with_civetweb"])],
		[AC_MSG_FAILURE(--with-civetweb was given, but test for civetweb failed)],
		[]
	)])
AM_CONDITIONAL([CUSTOMCIVETWEB], [test "x$with_civetweb" != x])

AC_ARG_ENABLE([glibshim],
	[AS_HELP_STRING([--enable-glibshim], [Enable shim for glib])]
	)
AM_CONDITIONAL([GLIBSHIM], [test "x$enable_glibshim" != "xyes"])	

AC_CONFIG_FILES([Makefile])

# Set C compiler
AC_PROG_CC
AC_OUTPUT
