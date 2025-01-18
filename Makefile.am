ACLOCAL_AMFLAGS = -I m4 --install
EXTRA_DIST = m4/NOTES
AM_DISTCHECK_CONFIGURE_FLAGS = CPPFLAGS=-I../../cJSON

bin_PROGRAMS = gonggo
gonggo_SOURCES = ./main/main.c
include ./cJSON/Makefile.am
include ./gear/Makefile.am

AM_CFLAGS = $(DEPS_CFLAGS) -I./cJSON -Wall -Werror
LIBS = $(DEPS_LIBS) -lcivetweb -lpthread -lrt

if CUSTOMCIVETWEB
AM_CFLAGS += -I$(CIVETWEBHOME)/include
LDADD = -L$(CIVETWEBHOME)
else
AM_CFLAGS += -I/usr/include/civetweb
endif
