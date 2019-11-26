
AC_DEFUN([AC_PACKAGE_NEED_HEADERS],
[
	AC_CHECK_HEADERS([ \
        limits.h \
        stddef.h \
        stdint.h \
        stdlib.h \
        stdbool.h \
        string.h \
        wchar.h \
        wctype.h \
        unistd.h \
        fcntl.h \
    	syslog.h \
    	search.h \
    	endian.h \
    	pthread.h \
        libgen.h \
    	sys/types.h \
    	sys/wait.h \
    	sys/time.h \
    	sys/prctl.h \
    	sys/mount.h \
    	sys/socket.h \
    	sys/file.h \
    	sys/stat.h \
    	sys/statvfs.h \
    	sys/resource.h \
    	sys/capability.h \
    	arpa/inet.h \
    	uuid/uuid.h \
    	netinet/in.h \
    	libmount/libmount.h \
    	libunwind.h], :, AC_MSG_ERROR([Unable to find header]))
])

AC_DEFUN([AC_PACKAGE_NEED_LIBS],
[
	AC_SEARCH_LIBS(pthread_create, pthread, :,
		AC_MSG_ERROR([Unable to find pthread]))

	AC_SEARCH_LIBS(sqrt, m, :,
		AC_MSG_ERROR([Unable to find libm]))

	AC_SEARCH_LIBS(mnt_new_context, mount, :,
		AC_MSG_ERROR([Unable to find libmount]))

	AC_SEARCH_LIBS(uuid_parse, uuid, :,
		AC_MSG_ERROR([Unable to find libuuid]))

	AC_SEARCH_LIBS(cap_clear, cap, :,
		AC_MSG_ERROR([Unable to find libcap]))

	AC_SEARCH_LIBS(fuse_reply_err, fuse, :,
		AC_MSG_ERROR([Unable to find libfuse]))

	AC_SEARCH_LIBS(unw_backtrace, unwind, :,
		AC_MSG_ERROR([Unable to find libunwind]))

	# TODO: here?
	PKG_CHECK_MODULES([FUSE], [fuse >= 2.7.4])
])
