-include $(top_builddir)/common.mk
SUBDIRS = . main

AM_CPPFLAGS = -I$(top_srcdir)/include

SUFFIXES = .ca
BUILT_SOURCES = $(nodist_libcauta_std_a_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

noinst_LIBRARIES = libcauta-std.a
nodist_libcauta_std_a_SOURCES = \
cauta-std/util/list.c \
$(NULL)

libcauta_std_a_SOURCES = \
    $(patsubst %.c,%.ca,$(nodist_libcauta_std_a_SOURCES))



