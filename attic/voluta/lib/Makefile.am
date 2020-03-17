-include $(top_builddir)/common.mk

libvolutadir = $(libdir)
libvoluta_LTLIBRARIES = libvoluta.la
libvoluta_la_LDFLAGS = $(NULL)
libvoluta_la_LDFLAGS += $(PTHREAD_CFLAGS) $(PTHREAD_DEF)
libvoluta_la_LDFLAGS += -L$(top_builddir)/lib -version-info 0
libvoluta_la_CPPFLAGS = $(NULL)
libvoluta_la_CPPFLAGS += -I$(top_srcdir)/include
libvoluta_la_CPPFLAGS += -DVOLUTA_LIB_MODULE=1
libvoluta_la_LIBADD = @LIBS@
libvoluta_la_LIBADD += $(PTHREAD_LIBS)
libvoluta_la_LIBADD += -lfuse3
libvoluta_la_SOURCES = $(NULL)
libvoluta_la_SOURCES += voluta-inline.h
libvoluta_la_SOURCES += voluta-lib.h
libvoluta_la_SOURCES += bringup.c
libvoluta_la_SOURCES += cache.c
libvoluta_la_SOURCES += crypto.c
libvoluta_la_SOURCES += dir.c
libvoluta_la_SOURCES += file.c
libvoluta_la_SOURCES += fusei.c
libvoluta_la_SOURCES += hash.c
libvoluta_la_SOURCES += inode.c
libvoluta_la_SOURCES += itable.c
libvoluta_la_SOURCES += namei.c
libvoluta_la_SOURCES += opers.c
libvoluta_la_SOURCES += panic.c
libvoluta_la_SOURCES += persist.c
libvoluta_la_SOURCES += qalloc.c
libvoluta_la_SOURCES += super.c
libvoluta_la_SOURCES += symlink.c
libvoluta_la_SOURCES += thread.c
libvoluta_la_SOURCES += utility.c
libvoluta_la_SOURCES += usyscall.c
libvoluta_la_SOURCES += version.c
libvoluta_la_SOURCES += volume.c
libvoluta_la_SOURCES += xattr.c
