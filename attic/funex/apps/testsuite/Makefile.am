include $(top_builddir)/common.mk
EXTRA_DIST = SConscript

AM_CPPFLAGS = -I$(top_srcdir)/include -I$(top_srcdir)

noinst_LIBRARIES = libfnxtests.a
libfnxtests_a_SOURCES = \
graybox.c \
graybox.h \
ioctls.c \
syscalls.c \
syscalls.h \
test-hooks.c \
test-hooks.h \
test-mio-shared.c \
test-mio-simple.c \
test-mio-unlinked.c \
test-mmap.c \
test-posix-access.c \
test-posix-chmod.c \
test-posix-create.c \
test-posix-fsync.c \
test-posix-link.c \
test-posix-mkdir.c \
test-posix-open.c \
test-posix-readdir.c \
test-posix-rename.c \
test-posix-stat.c \
test-posix-statvfs.c \
test-posix-symlink.c \
test-posix-unlink.c \
test-posix-write.c \
test-pseudo.c \
test-sio-basic.c \
test-sio-falloc.c \
test-sio-random.c \
test-sio-sequential.c \
test-sio-sparse.c \
test-sio-stat.c \
test-sio-truncate.c \
test-sio-unlinked.c \
$(NULL)
