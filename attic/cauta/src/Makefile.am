-include $(top_builddir)/common.mk
SUBDIRS = . main
SUFFIXES = .ca
BUILT_SOURCES = $(nodist_libcauta_std_la_SOURCES)
CLEANFILES = $(BUILT_SOURCES)

AM_CPPFLAGS = -I$(top_srcdir)/include
lib_LTLIBRARIES = libcauta-std.la

nodist_libcauta_std_la_SOURCES = \
scratchpad.c \
cauta-std/util/list.c \
$(NULL)

libcauta_std_la_SOURCES = \
  $(patsubst %.c,%.ca,$(nodist_libcauta_std_la_SOURCES))


.PHONY: generated
generated: scratchpad.c
-include generated

