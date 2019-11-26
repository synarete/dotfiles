
AC_DEFUN([CAC_NEED_HEADERS],
[
  AC_CHECK_HEADERS([ \
    gc.h \
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
    sys/types.h \
    sys/wait.h \
    sys/time.h \
    sys/prctl.h \
    sys/socket.h \
    sys/file.h \
    sys/stat.h \
    sys/statvfs.h \
    sys/resource.h \
    sys/capability.h \
    libunwind.h], :, AC_MSG_ERROR([Unable to find header]))
])

AC_DEFUN([CAC_NEED_LIBS],
[
  AX_PTHREAD

  AC_CHECK_LIB(m, sqrt, :,
    AC_MSG_ERROR([Unable to find libm]))

  AC_CHECK_LIB(gc, GC_init, :,
    AC_MSG_ERROR([Unable to find gc]))

  AC_CHECK_LIB(unwind, unw_backtrace, :,
    AC_MSG_ERROR([Unable to find libunwind]))
])

