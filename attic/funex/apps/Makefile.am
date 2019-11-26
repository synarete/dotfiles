include $(top_builddir)/common.mk
EXTRA_DIST = SConscript funex.conf.in

funex_confdir = $(sysconfdir)
funex_conf_DATA = funex.conf
CLEANFILES = $(funex_conf_DATA)

funex.conf: funex.conf.in

AM_CPPFLAGS = \
-I$(top_srcdir) \
-I$(top_srcdir)/include \
-I$(top_srcdir)/apps \
-DBUILDDATE="\"$(builddate)\"" \
-DRUNDIR="\"$(rundir)\"" \
$(NULL)

AM_LDFLAGS = -L$(top_builddir)/libfnx -L$(top_builddir)/apps

LDADD = -lfnxapp -lfnx

noinst_LIBRARIES = libfnxapp.a

libfnxapp_a_SOURCES = \
funex-app.h \
common/cmdline.c \
common/cmdline.h \
common/globals.c \
common/globals.h \
common/helpers.c \
common/helpers.h \
common/logging.c \
common/logging.h \
common/parse.c \
common/parse.h \
common/process.c \
common/process.h \
common/signals.c \
common/version.c \
common/version.h \
$(NULL)


bin_PROGRAMS = \
funex-fsd \
funex-mntd \
funex-test \
funex-show \
funex-dump \
funex-mount \
funex-mkfs \
$(NULL)

funex_fsd_SOURCES = funex-fsd.c

funex_mntd_SOURCES = funex-mntd.c

funex_show_SOURCES = funex-show.c

funex_dump_SOURCES = funex-dump.c

funex_mount_SOURCES = funex-mount.c

funex_mkfs_SOURCES = funex-mkfs.c

funex_test_SOURCES = \
funex-test.c \
testsuite/graybox.c \
testsuite/graybox.h \
testsuite/syscalls-ioctl.c \
testsuite/syscalls.c \
testsuite/syscalls.h \
testsuite/test-hooks.c \
testsuite/test-hooks.h \
testsuite/test-mio-shared.c \
testsuite/test-mio-simple.c \
testsuite/test-mio-unlinked.c \
testsuite/test-mmap.c \
testsuite/test-posix-access.c \
testsuite/test-posix-chmod.c \
testsuite/test-posix-create.c \
testsuite/test-posix-fsync.c \
testsuite/test-posix-link.c \
testsuite/test-posix-mkdir.c \
testsuite/test-posix-open.c \
testsuite/test-posix-readdir.c \
testsuite/test-posix-rename.c \
testsuite/test-posix-stat.c \
testsuite/test-posix-statvfs.c \
testsuite/test-posix-symlink.c \
testsuite/test-posix-unlink.c \
testsuite/test-posix-write.c \
testsuite/test-pseudo.c \
testsuite/test-sio-basic.c \
testsuite/test-sio-falloc.c \
testsuite/test-sio-random.c \
testsuite/test-sio-sequential.c \
testsuite/test-sio-sparse.c \
testsuite/test-sio-stat.c \
testsuite/test-sio-truncate.c \
testsuite/test-sio-unlinked.c \
$(NULL)


dist_sbin_SCRIPTS = \
binutils/mkfs.funex \
binutils/fsck.funex \
binutils/mount.funex \
binutils/umount.funex \
$(NULL)
